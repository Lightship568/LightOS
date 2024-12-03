#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define SUPER_NR 16                          // 超级块表16个
static super_block_t super_table[SUPER_NR];  // 超级块表
static super_block_t* root;                  // 根文件系统超级块

// 从超级块表中查找出一个空闲块
static super_block_t* get_free_super() {
    super_block_t* sb;
    for (size_t i = 0; i < SUPER_NR; ++i) {
        sb = &super_table[i];
        if (sb->dev == EOF) {
            return sb;
        }
    }
    panic("super block out of SUPER_NR %d\n", SUPER_NR);
}

super_block_t* get_super(dev_t dev) {
    super_block_t* sb;
    for (size_t i = 0; i < SUPER_NR; ++i) {
        sb = &super_table[i];
        if (sb->dev == dev) {
            sb->count++;
            return sb;
        }
    }
    return NULL;
}

void put_super(super_block_t* sb) {
    if (!sb) {
        return;
    }
    assert(sb->count > 0);
    sb->count--;
    if (sb->count) {
        return;
    }

    // 释放该超级块的imount、iroot、位图、以及super自身所在的缓存
    // 但设备本身仍然位于设备列表中，可以重新挂载
    sb->dev = EOF;
    iput(sb->imount);
    iput(sb->iroot);

    for (int i = 0; i < sb->desc->imap_blocks && i < IMAP_NR; ++i) {
        brelse(sb->imaps[i]);
    }
    for (int i = 0; i < sb->desc->zmap_blocks && i < ZMAP_NR; ++i) {
        brelse(sb->zmaps[i]);
    }
    brelse(sb->cache);
}

super_block_t* read_super(dev_t dev) {
    super_block_t* sb = get_super(dev);
    if (sb) {
        return sb;
    }

    LOGK("Reading super block of dev %d (%s)\n", dev, device_get(dev)->name);

    sb = get_free_super();

    sb->cache = bread(dev, FS_SUPER_BLOCK_NR);
    sb->desc = (super_desc_t*)sb->cache->data;
    sb->dev = dev;
    sb->count = 1;

    if (sb->desc->magic != MINIX_V1_MAGIC) {
        return NULL;
    }

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    int idx = FS_IMAP_BLOCK_NR;  // 读取块位图

    // 读取 inode 位图
    for (int i = 0; i < sb->desc->imap_blocks; ++i, ++idx) {
        assert(i < IMAP_NR);
        if (!(sb->imaps[i] = bread(dev, idx))) {
            panic(
                "Superblock bread error with return NULL\n");  // 当前的bread不会返回错误。
        }
    }
    // 读取数据块位图
    for (int i = 0; i < sb->desc->zmap_blocks; ++i, ++idx) {
        assert(i < ZMAP_NR);
        if (!(sb->zmaps[i] = bread(dev, idx))) {
            panic(
                "Superblock bread error with return NULL\n");  // 当前的bread不会返回错误。
        }
    }

    return sb;
}

static void mount_root() {
    LOGK("Root file system mounting...\n");
    // 假设主硬盘的第一个分区就是根文件系统
    device_t* device = device_find(DEV_IDE_PART, 0);
    assert(device);
    // 读根文件系统超级块
    root = read_super(device->dev);
    if(!root){
        panic("mount root failed: System first part of IDE device must be minix v1 file system!\n");
    }

    // 初始化根目录inode
    root->iroot = iget(device->dev, 1);
    // 根目录挂载点
    root->imount = iget(device->dev, 1);  // 增加引用计数

    root->iroot->mount = device->dev;

    LOGK("Root file system mounted\n");
}

int32 sys_mount(char* devname, char* dirname, int flags) {
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t* devinode = NULL;
    inode_t* dirinode = NULL;
    super_block_t* sb = NULL;
    devinode = namei(devname);
    if (!devinode) {
        goto clean;
    }
    if (!ISBLK(devinode->desc->mode)) {
        goto clean;
    }
    dev_t dev = devinode->desc->zone[0];
    dirinode = namei(dirname);
    if (!dirinode) {
        goto clean;
    }
    if (!ISDIR(dirinode->desc->mode)) {
        goto clean;
    }
    // 引用不是 1（避免目录被打开占用），或者已经挂载了设备
    if (dirinode->count != 1 || dirinode->mount) {
        goto clean;
    }

    // 读取超级块，设置其根目录，super 引用计数加一
    sb = read_super(dev);
    // 非 minix v1 文件系统，无法 mount
    if (!sb) {
        goto clean;
    }
    // 只能 mount 一次
    if (sb->imount) {
        goto clean;
    }
    sb->iroot = iget(dev, 1);
    sb->imount = dirinode;
    dirinode->mount = dev;
    iput(devinode);
    return 0;

clean:
    put_super(sb);
    iput(devinode);
    iput(dirinode);
    return EOF;
}

int32 sys_umount(char* target) {
    LOGK("umount %s\n", target);
    inode_t* inode = NULL;
    super_block_t* sb = NULL;
    int ret = EOF;

    inode = namei(target);
    if (!inode) {
        goto clean;
    }
    // 确保 target 是 devname 或非根目录的 dirname
    if (!ISBLK(inode->desc->mode) && inode->nr != 1) {
        goto clean;
    }
    // 不允许卸载系统根目录
    if (inode == root->imount) {
        goto clean;
    }
    // umount dirname
    dev_t dev = inode->dev;
    // umount devname
    if (ISBLK(inode->desc->mode)) {
        dev = inode->desc->zone[0];
    }

    // 不允许卸载 /dev 目录
    // todo: 调研高效?的保护机制
    if (device_find(DEV_RAMDISK, 0)->dev == dev){
        goto clean;
    }

    sb = get_super(dev);
    // 检查是否已经挂载
    if (!sb->imount) {
        goto clean;
    }
    // 确保被挂载的目录也有挂载信息
    if (!sb->imount->mount) {
        LOGK("Warning super block imount->mount = 0\n");
    }
    // 确保设备上没有正在被使用的文件
    if (list_size(&sb->inode_list) > 1) {
        goto clean;
    }
    // 释放 superblock
    iput(sb->iroot);
    sb->iroot = NULL;

    // 释放文件系统 dirname 的 inode
    sb->imount->mount = 0;
    iput(sb->imount);

    sb->imount = NULL;
    ret = 0;
    // 释放 mount 的引用计数
    put_super(sb);
clean:
    // 释放 umount 中的 get_super
    put_super(sb);
    iput(inode);
    return ret;
}

int32 devmkfs(dev_t dev, u32 icount) {
    super_block_t* sb = NULL;
    cache_t* pcache = NULL;
    int ret = EOF;

    // 检查是否被 mount
    sb = get_super(dev);
    if (sb) {
        if (sb->imount) {
            goto clean;
        }
        // 未 mount 则释放掉，后续重新设置其超级块
        put_super(sb);
        sb = NULL;
    }

    int total_block =
        device_ioctl(dev, DEV_CMD_SECTOR_COUNT, NULL, 0) / BLOCK_SECS;
    assert(total_block);
    assert(icount < total_block);
    // 若 icount 传入0，则默认 icount 数量为总块数的三分之一
    if (!icount) {
        icount = total_block / 3;
    }
    sb = get_free_super();
    sb->dev = dev;
    sb->count = 1;

    sb->cache = bread(dev, 1);  // 超级块缓存不释放
    sb->cache->dirty = true;

    // 初始化超级块
    super_desc_t* desc = (super_desc_t*)sb->cache->data;
    sb->desc = desc;

    desc->inodes = icount;
    // inode 所占的总块数
    int inode_blocks = div_round_up(icount * sizeof(inode_desc_t), BLOCK_SIZE);
    // 分别计算 imap 和 zmap 所占逻辑块大小
    desc->zones = total_block;
    desc->imap_blocks = div_round_up(icount, BLOCK_BITS);
    // 但这里 zcount 实际上多包含了 zmap 位图，小概率导致 zmap_blocks
    // 可能多一个块？影响不大
    int zcount =
        total_block - FS_IMAP_BLOCK_NR - desc->imap_blocks - inode_blocks;
    desc->zmap_blocks = div_round_up(zcount, BLOCK_BITS);

    desc->firstdatazone =
        FS_IMAP_BLOCK_NR + desc->imap_blocks + desc->zmap_blocks + inode_blocks;
    desc->log_zone_size = 0;
    desc->max_size = BLOCK_SIZE * TOTAL_BLOCKS;
    desc->magic = MINIX_V1_MAGIC;

    // 清空全部位图
    int idx = FS_IMAP_BLOCK_NR;
    for (int i = 0; i < (desc->imap_blocks + desc->zmap_blocks); i++, idx++) {
        pcache = bread(dev, idx);
        assert(pcache);
        if (i < desc->imap_blocks) {
            sb->imaps[i] = pcache;
        } else {
            sb->zmaps[i - desc->imap_blocks] = pcache;
        }
        pcache->dirty = true;
        memset(pcache->data, 0, BLOCK_SIZE);
        // brelse(pcache); 位图不可以释放，要跟随 super 的生命周期存在
    }

    // 初始化位图（zmap 首块与 imap 前两块直接占用）
    idx = balloc(dev);

    idx = ialloc(dev);
    idx = ialloc(dev);

    // 位图尾部有些没有用到，置位
    int counts[] = {
        icount + 1,
        zcount,
    };
    // 最后一块的位图
    cache_t* maps[] = {
        sb->imaps[sb->desc->imap_blocks - 1],
        sb->zmaps[sb->desc->zmap_blocks - 1],
    };
    for (size_t i = 0; i < 2; i++) {
        int count = counts[i];
        cache_t* map = maps[i];
        map->dirty = true;
        int offset = count % (BLOCK_BITS);
        int begin = (offset / 8);
        char* ptr = (char*)map->data + begin;
        memset(ptr + 1, 0xFF, BLOCK_SIZE - begin - 1);
        int bits = 0x80;
        char data = 0;
        int remain = 8 - offset % 8;
        while (remain--) {
            data |= bits;
            bits >>= 1;
        }
        ptr[0] = data;
    }
    // 创建根目录
    task_t* task = get_current();
    inode_t* iroot = new_inode(dev, 1);
    sb->iroot = iroot;

    iroot->desc->mode = (0777 & ~task->umask) | IFDIR;
    iroot->desc->size = sizeof(dentry_t) * 2;
    iroot->desc->nlinks = 2;

    pcache = bread(dev, bmap(iroot, 0, true));
    pcache->dirty = true;

    dentry_t* entry = (dentry_t*)pcache->data;
    memset(entry, 0, BLOCK_SIZE);
    strcpy(entry->name, ".");
    entry->nr = iroot->nr;

    entry++;
    strcpy(entry->name, "..");
    entry->nr = iroot->nr;

    brelse(pcache);
    ret = 0;

clean:
    put_super(sb);
    return ret;
}

int32 sys_mkfs(char* devname, int icount) {
    inode_t* inode = NULL;
    int ret = EOF;

    inode = namei(devname);
    if (!inode) {
        goto clean;
    }
    if (!ISBLK(inode->desc->mode)) {
        goto clean;
    }
    dev_t dev = inode->desc->zone[0];
    assert(dev);

    ret = devmkfs(dev, icount);

clean:
    iput(inode);
    return ret;
}

void super_init(void) {
    for (size_t i = 0; i < SUPER_NR; ++i) {
        super_block_t* sb = &super_table[i];
        sb->dev = EOF;
        sb->desc = NULL;
        sb->cache = NULL;
        sb->iroot = NULL;
        sb->imount = NULL;
        list_init(&sb->inode_list);
    }

    mount_root();
}
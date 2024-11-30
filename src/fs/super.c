#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/fs.h>
#include <sys/assert.h>
#include <lightos/stat.h>

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

    assert(sb->desc->magic == MINIX_V1_MAGIC);

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

    // 初始化根目录inode
    root->iroot = iget(device->dev, 1);
    // 根目录挂载点
    root->imount = iget(device->dev, 1);  // 增加引用计数

    root->iroot->mount = device->dev;

    LOGK("Root file system mounted\n");
}

int32 sys_mount(char* devname, char* dirname, int flags){
    LOGK("mount %s to %s\n", devname, dirname);

    inode_t* devinode = NULL;
    inode_t* dirinode = NULL;
    super_block_t* sb = NULL;
    devinode = namei(devname);
    if (!devinode){
        goto clean;
    }
    if (!ISBLK(devinode->desc->mode)){
        goto clean;
    }
    dev_t dev = devinode->desc->zone[0];
    dirinode = namei(dirname);
    if(!dirinode){
        goto clean;
    }
    if (!ISDIR(dirinode->desc->mode)){
        goto clean;
    }
    // 引用不是 1（避免目录被打开占用），或者已经挂载了设备
    if (dirinode->count != 1 || dirinode->mount){
        goto clean;
    }

    // 读取超级块，设置其根目录
    sb = read_super(dev);
    // 只能 mount 一次
    if (sb->imount){
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

int32 sys_umount(char* target){
    LOGK("umount %s\n", target);
    inode_t* inode = NULL;
    super_block_t* sb = NULL;
    int ret = EOF;

    inode = namei(target);
    if (!inode){
        goto clean;
    }
    // 确保 target 是 devname 或非根目录的 dirname
    if (!ISBLK(inode->desc->mode) && inode->nr != 1){
        goto clean;
    }
    // 不允许卸载系统根目录
    if (inode == root->imount){
        goto clean;
    }
    // umount dirname
    dev_t dev = inode->dev;
    // umount devname
    if (ISBLK(inode->desc->mode)){
        dev = inode->desc->zone[0];
    }

    sb = get_super(dev);
    // 检查是否已经挂载
    if (!sb->imount){
        goto clean;
    }
    // 确保被挂载的目录也有挂载信息
    if (!sb->imount->mount){
        LOGK("Warning super block imount->mount = 0\n");
    }
    // 确保设备上没有正在被使用的文件
    if (list_size(&sb->inode_list) > 1){
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
clean:
    put_super(sb);
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
#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/fs.h>
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

// 获取设备 dev 的超级块
super_block_t* get_super(dev_t dev) {
    super_block_t* sb;
    for (size_t i = 0; i < SUPER_NR; ++i) {
        sb = &super_table[i];
        if (sb->dev == dev) {
            return sb;
        }
    }
    return NULL;
}

// 读设备 dev 的超级块
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

    assert(sb->desc->magic == MINIX_V1_MAGIC);

    memset(sb->imaps, 0, sizeof(sb->imaps));
    memset(sb->zmaps, 0, sizeof(sb->zmaps));

    int idx = FS_IMAP_BLOCK_NR;  // 读取块位图

    // 读取 inode 位图
    for (int i = 0; i < sb->desc->imap_blocks; ++i, ++idx) {
        assert(i < IMAP_NR);
        if (!(sb->imaps[i] = bread(dev, idx))) {
            panic("Superblock bread error with return NULL\n");  // 当前的bread不会返回错误。
        }
    }
    // 读取数据块位图
    for (int i = 0; i < sb->desc->zmap_blocks; ++i, ++idx) {
        assert(i < ZMAP_NR);
        if (!(sb->zmaps[i] = bread(dev, idx))) {
            panic("Superblock bread error with return NULL\n");  // 当前的bread不会返回错误。
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
    root->imount = iget(device->dev, 1); // 增加引用计数

    LOGK("Root file system mounted\n");
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
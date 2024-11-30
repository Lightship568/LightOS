#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

#define INODE_NR 64

static inode_t inode_table[INODE_NR];

// 申请一个 inode
static inode_t* get_free_inode() {
    for (size_t i = 0; i < INODE_NR; ++i) {
        inode_t* inode = &inode_table[i];
        if (inode->dev == EOF) {
            return inode;
        }
    }
    panic("Inode table out of INODE_NR %d\n", INODE_NR);
}

// 释放一个 inode
static void put_free_inode(inode_t* inode) {
    assert(inode != inode_table);
    assert(inode->count == 0);
    inode->dev = EOF;
}

// 获取根 inode
inode_t* get_root_inode() {
    return inode_table;
}

// 计算 inode nr 所在的块号
static inline idx_t inode_block(super_block_t* sb, idx_t nr) {
    return FS_IMAP_BLOCK_NR + sb->desc->imap_blocks + sb->desc->zmap_blocks +
           (nr - 1) / BLOCK_INODES;
}

// 从已有 inode 中查找编号为 nr 的 inode
static inode_t* find_inode(dev_t dev, idx_t nr) {
    super_block_t* super = get_super(dev);
    assert(super);
    list_t* list = &super->inode_list;

    for (list_node_t* node = list->head.next; node != &list->tail;
         node = node->next) {
        inode_t* inode = element_entry(inode_t, node, node);
        if (inode->nr == nr) {
            return inode;
        }
    }
    return NULL;
}

// 如果 iget 尝试获取一个被 mount 的 inode
// 则需要将这个 inode 变化为该设备上的根目录
static inode_t* fit_inode(inode_t* inode) {
    if (!inode || !inode->mount) {
        return inode;
    }
    dev_t dev = inode->mount;
    super_block_t* sb = get_super(dev);
    assert(sb);
    iput(inode);
    inode = sb->iroot;
    inode->count++;
    return inode;
}

extern time_t sys_time(void);

// 获取 inode
inode_t* iget(dev_t dev, idx_t nr) {
    inode_t* inode = find_inode(dev, nr);
    if (inode) {
        inode->count++;
        inode->atime = sys_time();
        // 被 mount 的目录一定在 inode 链表中，因此只需要在此 fit_inode 即可
        return fit_inode(inode);
    }
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(nr <= sb->desc->inodes);

    inode = get_free_inode();
    inode->dev = dev;
    inode->nr = nr;
    // task_初始化 idle+init 的 iroot+ipwd 导致根 inode 已经增加了四个引用计数
    if (inode == get_root_inode()) {
        assert(inode->count == 4);
    } else {
        assert(inode->count == 0);
        inode->count = 1;
    }

    // 加入超级块 inode 链表
    list_push(&sb->inode_list, &inode->node);

    cache_t* pcache = bread(inode->dev, inode_block(sb, inode->nr));

    inode->cache = pcache;

    // 将缓存视为一个 inode 描述符数组，获取对应的指针。
    inode->desc =
        &((inode_desc_t*)pcache->data)[(inode->nr - 1) % BLOCK_INODES];

    inode->ctime = inode->desc->mtime;
    inode->atime = sys_time();

    return inode;
}

// 释放 inode
void iput(inode_t* inode) {
    if (!inode) {
        return;
    }

    if (inode->cache->dirty) {
        bwrite(inode->cache);  // 强一致
    }

    inode->count--;
    if (inode->count) {
        return;
    }

    // 释放 inode 对应的缓冲
    brelse(inode->cache);
    list_remove(&inode->node);
    put_free_inode(inode);
}

static void inode_bfree(inode_t* inode, u16* array, int index, int level) {
    if (!array[index]) {
        return;
    }

    // 直接块直接释放
    if (!level) {
        bfree(inode->dev, array[index]);
        return;
    }

    cache_t* pcache = bread(inode->dev, array[index]);
    for (size_t i = 0; i < BLOCK_INDEXES; ++i) {
        inode_bfree(inode, (u16*)pcache->data, i, level - 1);
    }
    brelse(pcache);
    bfree(inode->dev, array[index]);
}

void inode_truncate(inode_t* inode) {
    if (!ISFILE(inode->desc->mode) && !ISDIR(inode->desc->mode)) {
        return;
    }

    // 释放直接块
    for (size_t i = 0; i < DIRECT_BLOCKS; ++i) {
        inode_bfree(inode, inode->desc->zone, i, 0);
        inode->desc->zone[i] = 0;
    }

    // 释放一级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCKS, 1);
    inode->desc->zone[DIRECT_BLOCKS] = 0;

    // 释放二级间接块
    inode_bfree(inode, inode->desc->zone, DIRECT_BLOCKS + 1, 2);
    inode->desc->zone[DIRECT_BLOCKS + 1] = 0;

    // inode 设置
    inode->desc->size = 0;
    inode->desc->mtime = sys_time();

    // 强一致
    inode->cache->dirty = true;
    bwrite(inode->cache);
}

inode_t* new_inode(dev_t dev, idx_t nr) {
    task_t* task = get_current();
    inode_t* inode = iget(dev, nr);
    assert(inode->desc->nlinks == 0);

    inode->cache->dirty = true;

    inode->desc->mode = 0777 & (~task->umask);
    inode->desc->uid = task->uid;
    inode->desc->size = 0;
    inode->desc->mtime = inode->atime = sys_time();
    inode->desc->gid = task->gid;
    inode->desc->nlinks = 1;

    return inode;
}

void inode_init(void) {
    for (size_t i = 0; i < INODE_NR; ++i) {
        inode_t* inode = &inode_table[i];
        inode->dev = EOF;
        inode->count = 0;
    }
}
#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/fs.h>
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

extern time_t sys_time(void);

// 获取 inode
inode_t* iget(dev_t dev, idx_t nr) {
    inode_t* inode = find_inode(dev, nr);
    if (inode) {
        inode->count++;
        inode->atime = sys_time();
        return inode;
    }
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(nr <= sb->desc->inodes);

    inode = get_free_inode();
    inode->dev = dev;
    inode->nr = nr;
    inode->count = 1;

    // 加入超级块 inode 链表
    list_push(&sb->inode_list, &inode->node);

    cache_t* pcache = bread(inode->dev, inode_block(sb, inode->nr));

    inode->cache = pcache;

    // 将缓存视为一个 inode 描述符数组，获取对应的指针。
    inode->desc = &((inode_desc_t*)pcache->data)[(inode->nr - 1) % BLOCK_INODES];

    inode->ctime = inode->desc->mtime;
    inode->atime = sys_time();

    return inode;
}

// 释放 inode
void iput(inode_t* inode){
    if (!inode){
        return;
    }

    inode->count--;
    if (inode->count){
        return;
    }

    // 释放 inode 对应的缓冲
    brelse(inode->cache);
    list_remove(&inode->node);
    put_free_inode(inode);
}

void inode_init(void){
    for (size_t i =0 ; i < INODE_NR; ++i){
        inode_t* inode = &inode_table[i];
        inode->dev = EOF;
    }
}
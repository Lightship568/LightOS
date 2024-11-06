#ifndef LIGHTOS_BUFFER_H
#define LIGHTOS_BUFFER_H

#include <lib/list.h>
#include <lib/mutex.h>
#include <lightos/memory.h>

// 此处 SECTOR_SIZE 不应引用 IDE 设备，应做上层抽象来解耦合
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE * 2 // minix v1，块大小为1K
#define BLOCK_SECS (BLOCK_SIZE / SECTOR_SIZE)

typedef struct cache_t {
    char* data;         // 数据区
    dev_t dev;          // 设备号
    idx_t block;        // 块号
    int count;          // 引用计数
    list_node_t hnode;  // hash_table 哈希表拉链节点
    list_node_t rnode;  // free_list 空闲缓冲节点
    mutex_t lock;       // 锁
    bool dirty;         // 脏位
    bool valid;         // 是否有效
} cache_t;

cache_t* getblk(dev_t dev, idx_t block);
cache_t* bread(dev_t dev, idx_t block);
void bwrite(cache_t* cache);
void brelse(cache_t* cache);
void page_cache_init(void);

#endif
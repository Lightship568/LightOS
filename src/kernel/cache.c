#include <lightos/cache.h>
#include <lightos/device.h>
#include <lib/debug.h>
#include <sys/assert.h>

// #define LOGK(fmt, args...) DEBUGK(fmt, ##args)
#define LOGK(fmt, args...) ;

#define HASH_COUNT 101  // 素数

static cache_t* cache_start = (cache_t*)KERNEL_PAGE_CACHE_VADDR; // 缓存开始位置的虚拟地址（3G+8M）
static u32 cache_count = 0; // 当前cache 的数量

// 记录当前缓存结构体位置
static cache_t* cache_ptr = (cache_t*)KERNEL_PAGE_CACHE_VADDR;
// 记录当前数据缓冲区位置
static void* cache_data = (void*)(KERNEL_PAGE_CACHE_VADDR + KERNEL_PAGE_CAHCE_SIZE - BLOCK_SIZE);

list_t free_list;               // 空闲链表
list_t wait_list;               // 等待进程链表
list_t hash_table[HASH_COUNT];  // 缓存哈希表

// 哈希函数
u32 hash(dev_t dev, idx_t block) {
    return (dev ^ block) % HASH_COUNT;
}

// 找到
static cache_t* get_from_hash_table(dev_t dev, idx_t block){
    u32 idx = hash(dev, block);
    list_t* list = &hash_table[idx];
    cache_t* pcache = NULL;
    cache_t* ptr = NULL;
    for(list_node_t* node = list->head.next; node != &list->tail; node = node->next){
        // 查冲突拉链
        ptr = element_entry(cache_t, hnode, node);
        if (ptr->dev == dev && ptr->block == block){
            pcache = ptr;
            break;
        }
    }
    if (!pcache){
        return NULL;
    }
    // 将 pcache 从空闲列表中移除
    if (list_search(&free_list, &pcache->rnode)){
        list_remove(&pcache->rnode);
    }
    return pcache;
}

// 将 pcache 放入哈希表
static void hash_locate(cache_t* pcache){
    u32 idx = hash(pcache->dev, pcache->block);
    list_t* list = &hash_table[idx];
    assert(!list_search(list, &pcache->hnode));
    list_push(list, &pcache->hnode);
}

// 将 pcache 从哈希表移除
static void hash_remove(cache_t* pcache){
    u32 idx = hash(pcache->dev, pcache->block);
    list_t* list = &hash_table[idx];
    assert(list_search(list, &pcache->hnode));
    list_remove(&pcache->hnode);
}

// 若一次性链上所有 ptr-data 会非常费时，这里随取随链
static cache_t* get_new_cache(){
    cache_t* pcache = NULL;
    // 保证 cache 与 data 不会重叠
    if ((u32)cache_ptr + sizeof(cache_t) < (u32)cache_data) {
        pcache = cache_ptr;
        pcache->data = cache_data;
        pcache->dev = EOF;
        pcache->block = 0;
        pcache->count = 0;
        pcache->dirty = false;
        pcache->valid = false;
        mutex_init(&pcache->lock);
        cache_count++;
        cache_ptr++;
        cache_data -= BLOCK_SIZE;
        LOGK("Cache count %d\n", cache_count);
    }
    return pcache;
}

// 获得空闲的 cache
static cache_t* get_free_cache(){
    cache_t* pcache = NULL;
    while(true){
        // 尝试分配新的 ptr-data
        pcache = get_new_cache();
        if (pcache){
            return pcache;
        }
        // 若没有新的，则从空闲队列中找
        if (!list_empty(&free_list)){
            // 取最长时间未访问的缓冲块（LRU 最近最少使用）
            pcache = element_entry(cache_t, rnode, list_popback(&free_list));
            hash_remove(pcache);
            pcache->valid = false;
            return pcache;
        }
        // 否则，只能等待其他的缓冲释放
        task_block(get_current(), &wait_list, TASK_BLOCKED);
    }
}

// 获得设备 dev 第 block 对应的缓冲
cache_t* getblk(dev_t dev, idx_t block) {
    cache_t* pcache = get_from_hash_table(dev, block);
    if (pcache) {
        assert(pcache->valid);
        return pcache;
    }
    pcache = get_free_cache();
    assert(pcache->count == 0);
    assert(pcache->dirty == 0);
    pcache->count = 1;
    pcache->dev = dev;
    pcache->block = block;
    hash_locate(pcache);
    return pcache;
}

// 读取 dev 的 block 块
cache_t* bread(dev_t dev, idx_t block) {
    cache_t* pcache = getblk(dev, block);
    assert(pcache != NULL);
    if (pcache->valid){
        pcache->count++;
        return pcache;
    }

    device_request(pcache->dev, pcache->data, BLOCK_SECS, pcache->block * BLOCK_SECS, 0, REQUEST_READ);
    
    pcache->dirty = false;
    pcache->valid = true;
    return pcache;
}

// 写入缓冲块
void bwrite(cache_t* pcache) {
    assert(pcache);
    if (!pcache->dirty){
        return;
    }
    device_request(pcache->dev, pcache->data, BLOCK_SECS, pcache->block * BLOCK_SECS, 0, REQUEST_WRITE);
    pcache->dirty = false;
    pcache->valid = true;
}

// 释放缓冲
void brelse(cache_t* pcache) {
    if (!pcache){
        return;
    }
    pcache->count--;
    assert(pcache->count >= 0);
    if (pcache->count) // 还有引用计数，直接返回不释放
        return;

    // 理论上释放时，rnode不会加入任何链表
    assert(!pcache->rnode.next);
    assert(!pcache->rnode.prev);
    list_push(&free_list, &pcache->rnode);

    // 脏位需写入，当前启用强一致
    if (pcache->dirty){
        bwrite(pcache);
    }
    // 唤醒一个等待缓冲区的任务
    if (!list_empty(&wait_list)){
        task_t* task = element_entry(task_t, node, list_popback(&wait_list));
        task_intr_unblock_no_waiting_list(task);
    }
}

void page_cache_init() {
    list_init(&free_list);
    list_init(&wait_list);

    // 哈希表初始化
    for (size_t i = 0; i < HASH_COUNT; ++i) {
        list_init(&hash_table[i]);
    }
}
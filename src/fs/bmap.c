#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/cache.h>
#include <lightos/fs.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

// 分配一个文件块
idx_t balloc(dev_t dev) {
    super_block_t* sb = get_super(dev);
    assert(sb);

    cache_t* pcache = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < ZMAP_NR; ++i) {
        pcache = sb->zmaps[i];
        assert(pcache);

        // 将整个缓冲区作为位图
        bitmap_make(&map, pcache->data, BLOCK_SIZE,
                    sb->desc->firstdatazone - 1 + i * BLOCK_BITS);

        // 从位图中扫描一位
        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            // 如果扫描成功，则标记
            assert(bit < sb->desc->zones);
            pcache->dirty = true;
            break;
        }
    }
    // todo: 空间已满导致的扫描失败
    bwrite(pcache);  // 强一致
    return bit;
}

// 释放一个文件块
void bfree(dev_t dev, idx_t idx) {
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(idx < sb->desc->zones);

    cache_t* pcache = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    // 这里应该减去偏移再计算 block idx 对吧？
    size_t block_idx = (idx - (sb->desc->firstdatazone - 1)) / BLOCK_BITS;

    pcache = sb->zmaps[block_idx];
    assert(pcache);

    // 将整个缓冲区作为位图
    bitmap_make(&map, pcache->data, BLOCK_SIZE,
                sb->desc->firstdatazone - 1 + block_idx * BLOCK_BITS);

    // 将 idx 对应位图置0
    assert(bitmap_test(&map, idx));
    bitmap_set(&map, idx, 0);

    pcache->dirty = true;
    bwrite(pcache);  // 强一致
}

// 分配一个文件系统 inode
idx_t ialloc(dev_t dev) {
    super_block_t* sb = get_super(dev);
    assert(sb);

    cache_t* pcache = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    for (size_t i = 0; i < IMAP_NR; ++i) {
        pcache = sb->imaps[i];
        assert(pcache);

        // 将整个缓冲区作为位图
        bitmap_make(&map, pcache->data, BLOCK_SIZE, i * BLOCK_BITS);

        // 从位图中扫描一位
        bit = bitmap_scan(&map, 1);
        if (bit != EOF) {
            // 如果扫描成功，则标记
            assert(bit < sb->desc->zones);
            pcache->dirty = true;
            break;
        }
    }
    bwrite(pcache);  // 强一致
    return bit;
}

// 释放一个文件系统 inode
void ifree(dev_t dev, idx_t idx) {
    super_block_t* sb = get_super(dev);
    assert(sb);
    assert(idx < sb->desc->zones);

    cache_t* pcache = NULL;
    idx_t bit = EOF;
    bitmap_t map;

    size_t block_idx = idx / BLOCK_BITS;

    pcache = sb->imaps[block_idx];
    assert(pcache);

    // 将整个缓冲区作为位图
    bitmap_make(&map, pcache->data, BLOCK_SIZE, block_idx * BLOCK_BITS);

    // 将 idx 对应位图置0
    assert(bitmap_test(&map, idx));
    bitmap_set(&map, idx, 0);

    pcache->dirty = true;
    bwrite(pcache);  // 强一致
}

idx_t bmap(inode_t* inode, idx_t block, bool create){
    assert(block >= 0 && block < TOTAL_BLOCKS);
    // 将每一个level视作一个array，查找block偏移
    u16 index = block;
    u16* array = inode->desc->zone;
    cache_t* pcache = inode->cache;

    // 用于下面的 brelse, 传入参数 inode 的 cache 不应该释放
    pcache->count++;
    // 当前处理级别
    int level = 0;
    // 当前子级别块数量
    int divider = 1;

    // 直接块
    if (block < DIRECT_BLOCKS){
        goto reckon;
    }

    block -= DIRECT_BLOCKS;
    // 一级间接块
    if (block < INDIRECT1_BLOCKS){
        index = DIRECT_BLOCKS; // 设置为一级间接块
        level = 1;
        divider = 1;
        goto reckon;
    }
    // 二级间接块
    block -= INDIRECT1_BLOCKS;
    assert(block < INDIRECT2_BLOCKS);
    index = DIRECT_BLOCKS + 1; // 设置为二级间接块
    level = 2;
    divider = BLOCK_INDEXES;

reckon:
    for (; level >= 0; level--){
        // 如果不存在，且 create，则申请一块文件快
        if (!array[index] && create){
            array[index] = balloc(inode->dev);
            pcache->dirty = true;
        }

        brelse(pcache);

        // 如果 level == 0 或者 索引不存在，则直接返回
        if (level == 0 || !array[index]){
            return array[index];
        }

        // level 非 0， 处理下一级索引
        pcache = bread(inode->dev, array[index]);
        index = block / divider;
        block = block % divider;
        divider /= BLOCK_INDEXES;
        array = (u16*)pcache->data;
    }
}

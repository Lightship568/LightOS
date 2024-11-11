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

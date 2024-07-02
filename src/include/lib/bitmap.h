#ifndef LIGHTOS_BITMAP_H
#define LIGHTOS_BITMAP_H

#include <sys/types.h>

typedef struct bitmap_t {
    u8* bits;    // 位图缓冲区
    u32 length;  // 位图缓冲区长度
    u32 offset;  // 位图开始的偏移
} bitmap_t;

// 构造位图
void _bitmap_make(bitmap_t* map, char* bits, u32 length, u32 offset);

// 位图初始化，全部置为 0
void bitmap_init(bitmap_t* map, char* bits, u32 length, u32 offset);

// 测试某一位是否为 1
bool bitmap_test(bitmap_t* map, idx_t index);

// 设置位图某位的值
void bitmap_set(bitmap_t* map, idx_t index, bool value);

// 从位图中得到连续的 count 位
int bitmap_scan(bitmap_t* map, u32 count);

#endif
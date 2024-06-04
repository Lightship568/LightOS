#ifndef LIGHTOS_MEMORY_H
#define LIGHTOS_MEMORY_H

/**
 * 内存管理
*/

#include <sys/types.h>

#define PAGE_SIZE 0x1000        // 页大小 4 K
#define MEMORY_BASE 0x100000    // 1M 可用内存起始地址

void memory_init(u32 magic, u32 addr);

#endif
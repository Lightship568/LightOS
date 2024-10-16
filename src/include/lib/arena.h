#ifndef LIGHTOS_ARENA_H
#define LIGHTOS_ARENA_H

#include <sys/types.h>
#include <lib/list.h>

/**
 * 内核用堆管理器 kmalloc 与 kfree
 * 没法手写完善的堆管理器，简单实现一下
 * 页分配后根据请求进行拆分，释放时自动前后合并
 */

typedef struct arena_t {
    u32 magic : 30;         // 防止被溢出覆盖
    u32 is_used : 1;        // 使用标记
    u32 is_head : 1;        // 是否是页头
    struct arena_t* back_ptr;      // 物理相邻的上个 arena 指针，用于释放合并
    struct arena_t* forward_ptr;   // 物理相邻的下个 arena 指针，用于释放合并
    list_node_t list;       // 链表
    u32 length;             // buf 长度
    void* buf;              // 由于buf紧接着arena部分，所以这个buf没啥用
} arena_t;

void* kmalloc(size_t size);
void kfree(void *ptr);
void aerna_init(void);

#endif
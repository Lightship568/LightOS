#ifndef LIGHTOS_MEMORY_H
#define LIGHTOS_MEMORY_H

/**
 * 内存管理
*/

#include <sys/types.h>
#include <sys/global.h>

#define MEMORY_BASE 0x100000    // 1M 可用内存起始地址

#define KERNEL_PAGE_DIR_PADDR 0x0             // 页目录表起始位置（4页个页目录项）
#define KERNEL_PAGE_DIR_VADDR 0xC0000000// 虚拟地址位置
#define KERNEL_PAGE_TABLE (PAGE_SIZE)   // 页表PT起始（内存布局：[PD]-[PT]-[PT]...-[kernel_map]）
#define KERNEL_PAGE_TABLE_COUNT 2       // 页表页数量，2 页映射 8M 给内核
#define KERNEL_MEM_SIZE (PAGE_SIZE * (PAGE_SIZE / 4) * KERNEL_PAGE_TABLE_COUNT)                     // 内核内存大小 8M
#define KERNEL_MAP_BITS_VADDR (KERNEL_PAGE_DIR_VADDR + PAGE_SIZE * (KERNEL_PAGE_TABLE_COUNT + 1))   // 内核虚拟内存位图的起始位置，紧接着两个页表的布局。

// 用户栈顶地址 128M（受限于一页的vmap->buf限制）
#define USER_STACK_TOP 0x8000000

typedef struct page_entry_t {
    u8 present : 1;  // 在内存中
    u8 write : 1;    // 0 只读 1 可读可写
    u8 user : 1;     // 1 所有人 0 超级用户 DPL < 3
    u8 pwt : 1;      // page write through 1 直写模式，0 回写模式
    u8 pcd : 1;      // page cache disable 禁止该页缓冲
    u8 accessed : 1; // 被访问过，用于统计使用频率
    u8 dirty : 1;    // 脏页，表示该页缓冲被写过
    u8 pat : 1;      // page attribute table 页大小 4K/4M
    u8 global : 1;   // 全局，所有进程都用到了，该页不刷新缓冲
    u8 shared : 1;   // 共享内存页，与 CPU 无关
    u8 privat : 1;   // 私有内存页，与 CPU 无关
    u8 readonly : 1; // 只读内存页，与 CPU 无关
    u32 index : 20;  // 页索引
} _packed page_entry_t;

// 得到 cr3 寄存器
u32 get_cr3();

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde);    

// 主要是通过分析 bios 内存检测获取的内存信息计算页数据，参数来自 head.s 的 push
void memory_init(u32 magic, u32 addr);

// 初始化 mem_map
void memory_map_init(void);

// 初始化内存映射
void mapping_init(void);

// 将启动时设置的低地址页表映射清空
void unset_low_mapping(void);

// kmap 初始化
void kmap_init(void);

u32 kmap(u32 paddr);
u32 kunmap(u32 vaddr);

// 分配 count 个连续的内核页
u32 alloc_kpage(u32 count);

// 释放 count 个连续的内核页
void free_kpage(u32 vaddr, u32 count);

// 用户态内存映射管理
// 将用户态 vaddr 映射物理内存
void link_user_page(u32 vaddr);
// 取消用户态 vaddr(页对齐) 的物理内存映射（present=false）
void unlink_user_page(u32 vaddr);

#endif
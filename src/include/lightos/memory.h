#ifndef LIGHTOS_MEMORY_H
#define LIGHTOS_MEMORY_H

/**
 * 内存管理
*/

#include <sys/types.h>
#include <sys/global.h>
#include <lightos/task.h>
#include <lib/syscall.h>

#define MEMORY_BASE 0x100000    // 1M 可用内存起始地址

#define KERNEL_PAGE_DIR_PADDR 0x0                   // 页目录表起始位置（4页个页目录项）
#define KERNEL_PAGE_DIR_VADDR 0xC0000000            // 虚拟地址位置
#define KERNEL_VADDR_OFFSET KERNEL_PAGE_DIR_VADDR   // 内核虚拟地址偏移
#define KERNEL_PAGE_TABLE (PAGE_SIZE)   // 页表PT起始（内存布局：[PD]-[PT]-[PT]...-[kernel_map]）
#define KERNEL_PAGE_TABLE_COUNT 4       // 页表页数量，4 页映射 16M 给内核
#define KERNEL_MEM_SIZE (PAGE_SIZE * (PAGE_SIZE / 4) * KERNEL_PAGE_TABLE_COUNT)                     // 内核内存大小 16M
#define KERNEL_MAP_BITS_VADDR (KERNEL_PAGE_DIR_VADDR + PAGE_SIZE * (KERNEL_PAGE_TABLE_COUNT + 1))   // 内核虚拟内存位图的起始位置，紧接着4个页表的布局。

#define KERNEL_PAGE_CACHE_VADDR (KERNEL_VADDR_OFFSET + 0x800000) // 8M-12M 为 PAGE_CACHE 缓冲区
#define KERNEL_PAGE_CAHCE_SIZE 0x400000

#define KERNEL_RAMDISK_VADDR (KERNEL_PAGE_CACHE_VADDR + KERNEL_PAGE_CAHCE_SIZE) // 12M-16M 为 RAMDISK
#define KERNEL_RAMDISK_SIZE 0x400000

// 用户栈顶地址 256M（不使用 vmap 跟踪，不受 128M 上限限制）
#define USER_STACK_TOP (0x10000000 - 1)
// 用户栈底地址，最大栈 2M，[0xFE00000, 0x10000000)
#define USER_STACK_BOTTOM (USER_STACK_TOP - 0x200000)
// 用户程序起始地址
#define USER_EXEC_ADDR 0
// 用户映射内存开始位置 128M
#define USER_MMAP_ADDR 0x8000000
// 用户映射内存大小 126M [128M, 254M)
#define USER_MMAP_SIZE (USER_STACK_BOTTOM - USER_MMAP_ADDR)

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

// 设置 cr3 寄存器，参数是页目录的地址
void set_cr3(u32 pde);    

// 拷贝 current 进程 pde 到 target_task->pde
void copy_pde(task_t* target_task);
// 拷贝 current 进程 pd 和所有有效 pt 到 target_task（并设置pde）
// 同时增加 >=1M 的 mmap 引用
void copy_pte(task_t* target_task);

// 对应 copy_pte，在程序exit时调用
// 释放所有user_page, PTs, PD, 解引用内核共享mem_map（归零自动释放）
void free_pte(task_t* target_task);

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

void page_fault(int vector, u32 edi, u32 esi, u32 ebp, u32 esp, u32 ebx, u32 edx, u32 ecx,
    u32 eax, u32 gs, u32 fs, u32 es, u32 ds, u32 vector0, u32 error, u32 eip, u32 cs, u32 eflags, u32 esp3, u32 ss3);

// 系统调用 brk
int32 sys_brk(void* addr);

// syscall: mmap & munmap
void* sys_mmap(mmap_args* args);
int sys_munmap(void* addr, size_t length);

#endif
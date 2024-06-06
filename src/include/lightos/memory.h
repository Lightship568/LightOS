#ifndef LIGHTOS_MEMORY_H
#define LIGHTOS_MEMORY_H

/**
 * 内存管理
*/

#include <sys/types.h>
#include <sys/global.h>

#define MEMORY_BASE 0x100000    // 1M 可用内存起始地址

#define KERNEL_PAGE_DIR 0x0         // 页目录表起始位置（4页个页目录项），学 linux 指向 0，让内核物理线性相等
#define KERNEL_PAGE_DIR_COUNT 4     // 页目录的页数量
#define KERNEL_PAGE_TABLE (PAGE_SIZE * KERNEL_PAGE_DIR_COUNT)    // 0x1000*4 页表起始，数量为 total_pages 个
#define KERNEL_PAGE_TABLE_COUNT 2   // 页表页数量，2 页映射 8M 给内核
#define KERNEL_MEM_SIZE (PAGE_SIZE * (PAGE_SIZE / 4) * KERNEL_PAGE_TABLE_COUNT)           // 内核内存大小 8M


typedef struct page_entry_t
{
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


#endif
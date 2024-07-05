#ifndef LIGHTOS_GLOBAL_H
#define LIGHTOS_GLOBAL_H

/**
 * GDT TSS 相关数据结构和设置函数
 */

#include <sys/types.h>

#define PAGE_SIZE 4096

#define GDT_SIZE 128

#define KERNEL_CODE_IDX 1
#define KERNEL_DATA_IDX 2
#define KERNEL_TSS_IDX 3

#define USER_CODE_IDX 4
#define USER_DATA_IDX 5

#define KERNEL_CODE_SELECTOR (KERNEL_CODE_IDX << 3)
#define KERNEL_DATA_SELECTOR (KERNEL_DATA_IDX << 3)
#define KERNEL_TSS_SELECTOR (KERNEL_TSS_IDX << 3)

#define USER_CODE_SELECTOR (USER_CODE_IDX << 3 | 0b11)
#define USER_DATA_SELECTOR (USER_DATA_IDX << 3 | 0b11)

// 全局描述符
typedef struct descriptor_t /* 共 8 个字节 */
{
    unsigned short limit_low;    // 段界限 0 ~ 15 位
    unsigned int base_low : 24;  // 基地址 0 ~ 23 位 16M
    unsigned char type : 4;      // 段类型
    unsigned char segment : 1;  // 1 表示代码段或数据段，0 表示系统段
    unsigned char DPL : 2;  // Descriptor Privilege Level 描述符特权等级 0 ~ 3
    unsigned char present : 1;  // 存在位，1 在内存中，0 在磁盘上
    unsigned char limit_high : 4;  // 段界限 16 ~ 19;
    unsigned char available : 1;  // 该安排的都安排了，送给操作系统吧
    unsigned char long_mode : 1;    // 64 位扩展标志
    unsigned char big : 1;          // 32 位 还是 16 位;
    unsigned char granularity : 1;  // 粒度 4KB 或 1B
    unsigned char base_high;        // 基地址 24 ~ 31 位
} _packed descriptor_t;

// 段选择子
typedef struct selector_t {
    u8 RPL : 2;  // Request Privilege Level
    u8 TI : 1;   // Table Indicator
    u16 index : 13;
} selector_t;

// 全局描述符表指针
typedef struct pointer_t {
    u16 limit;
    u32 base;
} _packed pointer_t;

typedef struct tss_t {
    u32 backlink;    // 前一个任务的链接，保存了前一个任务状态段的段选择子
    u32 esp0;        // ring0 的栈顶地址
    u32 ss0;         // ring0 的栈段选择子
    u32 esp1;        // ring1 的栈顶地址
    u32 ss1;         // ring1 的栈段选择子
    u32 esp2;        // ring2 的栈顶地址
    u32 ss2;         // ring2 的栈段选择子
    u32 cr3;         // 页目录表基址寄存器
    u32 eip;         // 指令指针
    u32 eflags;      // 标志寄存器
    u32 eax;         // 通用寄存器
    u32 ecx;
    u32 edx;
    u32 ebx;
    u32 esp;         // 堆栈指针
    u32 ebp;         // 基址指针
    u32 esi;         // 源变址寄存器
    u32 edi;         // 目的变址寄存器
    u32 es;          // 段寄存器
    u32 cs;
    u32 ss;
    u32 ds;
    u32 fs;
    u32 gs;
    u32 ldtr;        // 局部描述符选择子
    u16 trace : 1;   // 如果置位，任务切换时将引发一个调试异常
    u16 reserved : 15;  // 保留不用
    u16 iobase;      // I/O 位图基地址，16 位从 TSS 到 IO 权限位图的偏移
    u16 ssp;         // 任务影子栈指针
} __attribute__((packed)) tss_t;


void gdt_init(void);

#endif
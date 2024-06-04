#ifndef LIGHTOS_INTERRUPT_H
#define LIGHTOS_INTERRUPT_H

/**
 * 中断
 * 包括中断门（系统调用）、陷阱门的设置
 *
 */

#include <sys/types.h>

#define IDT_SIZE 256
// 陷阱入口指针表大小
#define TRAP_TABLE_SIZE 0x30

#define INTR_DE 0    // 除零错误
#define INTR_DB 1    // 调试
#define INTR_NMI 2   // 不可屏蔽中断
#define INTR_BP 3    // 断点
#define INTR_OF 4    // 溢出
#define INTR_BR 5    // 越界
#define INTR_UD 6    // 指令无效
#define INTR_NM 7    // 协处理器不可用
#define INTR_DF 8    // 双重错误
#define INTR_OVER 9  // 协处理器段超限
#define INTR_TS 10   // 无效任务状态段
#define INTR_NP 11   // 段无效
#define INTR_SS 12   // 栈段错误
#define INTR_GP 13   // 一般性保护异常
#define INTR_PF 14   // 缺页错误
#define INTR_RE1 15  // 保留
#define INTR_MF 16   // 浮点异常
#define INTR_AC 17   // 对齐检测
#define INTR_MC 18   // 机器检测
#define INTR_XM 19   // SIMD 浮点异常
#define INTR_VE 20   // 虚拟化异常
#define INTR_CP 21   // 控制保护异常

#define IRQ_CLOCK 0       // 时钟
#define IRQ_KEYBOARD 1    // 键盘
#define IRQ_CASCADE 2     // 8259 从片控制器
#define IRQ_SERIAL_2 3    // 串口 2
#define IRQ_SERIAL_1 4    // 串口 1
#define IRQ_PARALLEL_2 5  // 并口 2
#define IRQ_SB16 5        // SB16 声卡
#define IRQ_FLOPPY 6      // 软盘控制器
#define IRQ_PARALLEL_1 7  // 并口 1
#define IRQ_RTC 8         // 实时时钟
#define IRQ_REDIRECT 9    // 重定向 IRQ2
#define IRQ_NIC 11        // 网卡
#define IRQ_MOUSE 12      // 鼠标
#define IRQ_MATH 13       // 协处理器 x87
#define IRQ_HARDDISK 14   // ATA 硬盘第一通道
#define IRQ_HARDDISK2 15  // ATA 硬盘第二通道

#define IRQ_MASTER_NR 0x20  // 主片起始向量号
#define IRQ_SLAVE_NR 0x28   // 从片起始向量号

typedef struct gate_t {
    u16 offset0;     // 段内偏移 0 ~ 15 位
    u16 selector;    // 代码段选择子
    u8 reserved;     // 保留不用
    u8 type : 4;     // 任务门/中断门/陷阱门
    u8 segment : 1;  // segment = 0 表示系统段
    u8 DPL : 2;      // 使用 int 指令访问的最低权限
    u8 present : 1;  // 是否有效
    u16 offset1;     // 段内偏移 16 ~ 31 位
} _packed gate_t;

typedef void* handler_t;  // 中断处理函数

// 中断初始化，包括 IDT 的初步设置
void interrupt_init(void);

// 通知中断控制器，中断处理结束
void send_eoi(int vector);

void syscall_0(void);

// 清除 IF 位，返回设置之前的值
bool interrupt_disable(void);

// 获得 IF 位
bool get_interrupt_state(void);

// 开中断
void start_interrupt(void);

// 关中断
void close_interrupt(void);

// 注册异常处理函数
void set_exception_handler(u32 intr, handler_t handler);

// 注册中断处理函数
void set_interrupt_handler(u32 irq, handler_t handler);

// 设置中断mask，即是否禁用
void set_interrupt_mask(u32 irq, bool enable);

#endif
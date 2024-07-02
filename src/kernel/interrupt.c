#include <lib/debug.h>
#include <lib/io.h>
#include <lib/print.h>
#include <lightos/interrupt.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/global.h>

gate_t idt[IDT_SIZE];
pointer_t idt_ptr;

// 陷阱入口指针表，这个表是陷阱中断的入口
// 由于cpu触发陷阱中断有的会压栈错误码，有的不会
// 因此在汇编中做了参数压栈平衡，之后再调用c中的相应处理函数（trap_handler_table）
// 定义在interrupt_handler.asm中的中断处理函数入口表
extern handler_t trap_entry_table[TRAP_TABLE_SIZE];
// 这个是用于trap后续处理的函数，前面的只是entry，后面会跳转到trap_handler_table中的相应位置
handler_t trap_handler_table[TRAP_TABLE_SIZE];
// 忽略的中断处理，用于初始化全部中断
extern void ignore_interrupt();

extern handler_t* syscall_entry(void);

// 外中断相关
#define PIC_M_CTRL 0x20  // 主片的控制端口
#define PIC_M_DATA 0x21  // 主片的数据端口
#define PIC_S_CTRL 0xa0  // 从片的控制端口
#define PIC_S_DATA 0xa1  // 从片的数据端口
#define PIC_EOI 0x20     // 通知中断控制器中断结束

void send_eoi(int vector) {
    if (vector >= 0x20 && vector < 0x28) {  // 主片
        outb(PIC_M_CTRL, PIC_EOI);
    }
    if (vector >= 0x28 && vector < 0x30) {  // 主+从片
        outb(PIC_M_CTRL, PIC_EOI);
        outb(PIC_S_CTRL, PIC_EOI);
    }
}

void set_exception_handler(u32 intr, handler_t handler) {
    assert(intr >= 0 && intr <= 17);
    trap_handler_table[intr] = handler;
}

void set_interrupt_handler(u32 irq, handler_t handler) {
    assert(irq >= 0 && irq < 16);
    trap_handler_table[IRQ_MASTER_NR + irq] = handler;
}

void set_interrupt_mask(u32 irq, bool enable) {
    assert(irq >= 0 && irq < 16);
    u16 port;
    if (irq < 8) {
        port = PIC_M_DATA;
    } else {
        port = PIC_S_DATA;
        irq -= 8;
    }
    if (enable) {
        outb(port, inb(port) & ~(1 << irq));
    } else {
        outb(port, inb(port) | (1 << irq));
    }
}

void pic_init() {
    outb(PIC_M_CTRL, 0b00010001);  // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_M_DATA, 0x20);        // ICW2: 起始中断向量号 0x20
    outb(PIC_M_DATA, 0b00000100);  // ICW3: IR2接从片.
    outb(PIC_M_DATA, 0b00000001);  // ICW4: 8086模式, 正常EOI

    outb(PIC_S_CTRL, 0b00010001);  // ICW1: 边沿触发, 级联 8259, 需要ICW4.
    outb(PIC_S_DATA, 0x28);        // ICW2: 起始中断向量号 0x28
    outb(PIC_S_DATA, 2);  // ICW3: 设置从片连接到主片的 IR2 引脚
    outb(PIC_S_DATA, 0b00000001);  // ICW4: 8086模式, 正常EOI

    outb(PIC_M_DATA, 0b11111111);  // 关闭所有中断
    outb(PIC_S_DATA, 0b11111111);  // 关闭所有中断
}

bool interrupt_disable(void) {
    asm volatile(
        "pushfl\n"         // 将当前 eflags 压入栈中
        "cli\n"            // 清除 IF 位，此时外中断已被屏蔽
        "popl %eax\n"      // 将刚才压入的 eflags 弹出到 eax
        "shrl $9, %eax\n"  // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n"  // 只需要 IF 位
    );
}

bool get_interrupt_state(void) {
    asm volatile(
        "pushfl\n"         // 将当前 eflags 压入栈中
        "popl %eax\n"      // 将压入的 eflags 弹出到 eax
        "shrl $9, %eax\n"  // 将 eax 右移 9 位，得到 IF 位
        "andl $1, %eax\n"  // 只需要 IF 位
    );
}

void set_interrupt_state(bool state){
    if (state) start_interrupt();
    else close_interrupt();
}

void start_interrupt(void) {
    DEBUGK("sti\n");
    asm volatile("sti\n");
}

void close_interrupt(void) {
    DEBUGK("sti\n");
    asm volatile("cli\n");
}

static char* messageList[] = {
    "#DE Divide Error\0",
    "#DB RESERVED\0",
    "--  NMI Interrupt\0",
    "#BP Breakpoint\0",
    "#OF Overflow\0",
    "#BR BOUND Range Exceeded\0",
    "#UD Invalid Opcode (Undefined Opcode)\0",
    "#NM Device Not Available (No Math Coprocessor)\0",
    "#DF Double Fault\0",
    "    Coprocessor Segment Overrun (reserved)\0",
    "#TS Invalid TSS\0",
    "#NP Segment Not Present\0",
    "#SS Stack-Segment Fault\0",
    "#GP General Protection\0",
    "#PF Page Fault\0",
    "--  (Intel reserved. Do not use.)\0",
    "#MF x87 FPU Floating-Point Error (Math Fault)\0",
    "#AC Alignment Check\0",
    "#MC Machine Check\0",
    "#XF SIMD Floating-Point Exception\0",
    "#VE Virtualization Exception\0",
    "#CP Control Protection Exception\0",
};

// 异常处理函数（IDT 0x0-0x1f）
void exception_handler(int vector,
                       u32 edi,
                       u32 esi,
                       u32 ebp,
                       u32 esp,
                       u32 ebx,
                       u32 edx,
                       u32 ecx,
                       u32 eax,
                       u32 gs,
                       u32 fs,
                       u32 es,
                       u32 ds,
                       u32 vector0,
                       u32 error,
                       u32 eip,
                       u32 cs,
                       u32 eflags) {
    char* message = NULL;
    if (vector < 22) {
        message = messageList[vector];
    } else {
        message = messageList[15];
    }
    printk("\nEXCEPTION : %s \n", message);
    printk("   VECTOR : 0x%02X\n", vector);
    printk("    ERROR : 0x%08X\n", error);
    printk("   EFLAGS : 0x%08X\n", eflags);
    printk("       CS : 0x%02X\n", cs);
    printk("      EIP : 0x%08X\n", eip);
    printk("      ESP : 0x%08X\n", esp);

    bool hanging = true;
    // 阻塞
    while (hanging)
        ;
    // 通过 EIP 的值应该可以找到出错的位置
    // 也可以在出错时，可以将 hanging 在调试器中手动设置为 0
    // 然后在下面 return 打断点，单步调试，找到出错的位置
    return;
}

// 外中断处理函数（IDT 0X20-0x2f）
u32 counter = 0;
void outer_interrupt_handler(int vector) {
    send_eoi(vector);
    DEBUGK("[0x%x] outer interrupt %d...\n", vector, counter++);
}

void syscall_0(void) {
    DEBUGK("SYSCALL: 0x80 syscall called...\n");
}

void default_handler(int vector) {
    panic("Interrupt: [0x%2X] default interrupt\n", vector);
}

void idt_init(void) {
    gate_t* gate;
    handler_t handler;
    for (size_t i = 0; i < IDT_SIZE; ++i) {
        gate = &idt[i];
        if (i < TRAP_TABLE_SIZE) {  // 0x30 个 trap handler
            handler = trap_entry_table[i];
        } else {  // ingore interrupt
            handler = default_handler;
        }
        gate->offset0 = (u32)handler & 0xffff;
        gate->offset1 = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3;  // 代码段
        gate->reserved = 0;
        gate->type = 0b1110;  // 中断门
        gate->segment = 0;    // 系统段
        gate->DPL = 0;        // 内核态
        gate->present = 1;    // 有效
    }
    // 将trap handler补全（entry+handler）
    for (size_t i = 0; i < 0x20; ++i) {
        trap_handler_table[i] = exception_handler;
    }
    // 将外中断补全
    for (size_t i = 0x20; i < TRAP_TABLE_SIZE; ++i) {
        trap_handler_table[i] = outer_interrupt_handler;
    }

    // 设置syscall
    gate = &idt[0x80];
    gate->offset0 = (u32)syscall_entry & 0xffff;
    gate->offset1 = ((u32)syscall_entry >> 16) & 0xffff;
    gate->selector = 1 << 3;  // 代码段
    gate->reserved = 0;
    gate->type = 0b1110;  // 中断门
    gate->segment = 0;    // 系统段
    gate->DPL = 3;        // 内核态
    gate->present = 1;    // 有效

    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr\n");
}

void interrupt_init() {
    pic_init();
    idt_init();
    DEBUGK("Interrupt Initialized\n");
}

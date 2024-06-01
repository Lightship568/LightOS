#include <sys/interrupt.h>
#include <sys/global.h>
#include <lib/debug.h>
#include <lib/assert.h>

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

extern handler_t *syscall_entry(void);

static char *messageList[] = {
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

void exception_handler(int vector){
    char *message = NULL;
    if (vector < 22){
        message = messageList[vector];
    }else{
        message = messageList[15];
    }
    DEBUGK("Exception: [0x%2X] %s\n", vector, message);
}

void default_handler(int vector){
    panic("Interrupt: [0x%2X] default interrupt called...\n", vector);
}

void syscall_0(void){
    DEBUGK("SYSCALL: 0x80 syscall called...\n");
}

void interrupt_init(void){
    gate_t *gate;
    handler_t handler;
    for (size_t i = 0; i < IDT_SIZE; ++i){
        gate = &idt[i];
        if (i < TRAP_TABLE_SIZE){   // 0x30个trap handler
            handler = trap_entry_table[i];
        }else if (i == 0x80){       // syscall
            handler = syscall_entry;
        }else{                      // ingore interrupt
            handler = default_handler;
        }
        gate->offset0 = (u32)handler & 0xffff;
        gate->offset1 = ((u32)handler >> 16) & 0xffff;
        gate->selector = 1 << 3;    //代码段
        gate->reserved = 0;
        gate->type = 0b1110;        //中断门
        gate->segment = 0;          //系统段
        gate->DPL = 0;              //内核态
        gate->present = 1;          //有效
    }
    // 将trap handler补全（entry+handler）
    for (size_t i = 0; i < TRAP_TABLE_SIZE; ++i){
        trap_handler_table[i] = exception_handler;
    }
    idt_ptr.base = (u32)idt;
    idt_ptr.limit = sizeof(idt) - 1;
    asm volatile("lidt idt_ptr\n");
}



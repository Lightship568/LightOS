[bits 32]
; 中断处理函数入口

extern trap_handler_table
extern printk
extern syscall_0

section .text

%macro TRAP_HANDLER 2
TRAP_HANDLER_%1:
%ifn %2
    push 0 ; 有的trap会自动压入错误码，为无参数的trap也压入，方便后续统一处理
%endif
    push %1; 压入中断向量，跳转到中断入口
    jmp interrupt_entry
%endmacro

interrupt_entry:
    mov eax, [esp]
    call [trap_handler_table + eax * 4]
    add esp, 8
    iret

global syscall_entry
syscall_entry:
    call syscall_0
    iret

; 宏展开，第一个参数为trap编号，第二个参数表示cpu是否自动压入错误码
TRAP_HANDLER 0x00, 0; divide by zero
TRAP_HANDLER 0x01, 0; debug
TRAP_HANDLER 0x02, 0; non maskable interrupt
TRAP_HANDLER 0x03, 0; breakpoint

TRAP_HANDLER 0x04, 0; overflow
TRAP_HANDLER 0x05, 0; bound range exceeded
TRAP_HANDLER 0x06, 0; invalid opcode
TRAP_HANDLER 0x07, 0; device not avilable

TRAP_HANDLER 0x08, 1; double fault
TRAP_HANDLER 0x09, 0; coprocessor segment overrun
TRAP_HANDLER 0x0a, 1; invalid TSS
TRAP_HANDLER 0x0b, 1; segment not present

TRAP_HANDLER 0x0c, 1; stack segment fault
TRAP_HANDLER 0x0d, 1; general protection fault
TRAP_HANDLER 0x0e, 1; page fault
TRAP_HANDLER 0x0f, 0; reserved

TRAP_HANDLER 0x10, 0; x87 floating point exception
TRAP_HANDLER 0x11, 1; alignment check
TRAP_HANDLER 0x12, 0; machine check
TRAP_HANDLER 0x13, 0; SIMD Floating - Point Exception

TRAP_HANDLER 0x14, 0; Virtualization Exception
TRAP_HANDLER 0x15, 1; Control Protection Exception
TRAP_HANDLER 0x16, 0; reserved
TRAP_HANDLER 0x17, 0; reserved

TRAP_HANDLER 0x18, 0; reserved
TRAP_HANDLER 0x19, 0; reserved
TRAP_HANDLER 0x1a, 0; reserved
TRAP_HANDLER 0x1b, 0; reserved

TRAP_HANDLER 0x1c, 0; reserved
TRAP_HANDLER 0x1d, 0; reserved
TRAP_HANDLER 0x1e, 0; reserved
TRAP_HANDLER 0x1f, 0; reserved

TRAP_HANDLER 0x20, 0; clock 时钟中断
TRAP_HANDLER 0x21, 0; keyboard 键盘中断
TRAP_HANDLER 0x22, 0; cascade 级联 8259
TRAP_HANDLER 0x23, 0; com2 串口2
TRAP_HANDLER 0x24, 0; com1 串口1
TRAP_HANDLER 0x25, 0; sb16 声霸卡
TRAP_HANDLER 0x26, 0; floppy 软盘
TRAP_HANDLER 0x27, 0
TRAP_HANDLER 0x28, 0; rtc 实时时钟
TRAP_HANDLER 0x29, 0
TRAP_HANDLER 0x2a, 0
TRAP_HANDLER 0x2b, 0; nic 网卡
TRAP_HANDLER 0x2c, 0
TRAP_HANDLER 0x2d, 0
TRAP_HANDLER 0x2e, 0; harddisk1 硬盘主通道
TRAP_HANDLER 0x2f, 0; harddisk2 硬盘从通道

; 下面的数组记录了每个中断入口函数的指针
section .data
global trap_entry_table
trap_entry_table:
    dd TRAP_HANDLER_0x00
    dd TRAP_HANDLER_0x01
    dd TRAP_HANDLER_0x02
    dd TRAP_HANDLER_0x03
    dd TRAP_HANDLER_0x04
    dd TRAP_HANDLER_0x05
    dd TRAP_HANDLER_0x06
    dd TRAP_HANDLER_0x07
    dd TRAP_HANDLER_0x08
    dd TRAP_HANDLER_0x09
    dd TRAP_HANDLER_0x0a
    dd TRAP_HANDLER_0x0b
    dd TRAP_HANDLER_0x0c
    dd TRAP_HANDLER_0x0d
    dd TRAP_HANDLER_0x0e
    dd TRAP_HANDLER_0x0f
    dd TRAP_HANDLER_0x10
    dd TRAP_HANDLER_0x11
    dd TRAP_HANDLER_0x12
    dd TRAP_HANDLER_0x13
    dd TRAP_HANDLER_0x14
    dd TRAP_HANDLER_0x15
    dd TRAP_HANDLER_0x16
    dd TRAP_HANDLER_0x17
    dd TRAP_HANDLER_0x18
    dd TRAP_HANDLER_0x19
    dd TRAP_HANDLER_0x1a
    dd TRAP_HANDLER_0x1b
    dd TRAP_HANDLER_0x1c
    dd TRAP_HANDLER_0x1d
    dd TRAP_HANDLER_0x1e
    dd TRAP_HANDLER_0x1f
    dd TRAP_HANDLER_0x20
    dd TRAP_HANDLER_0x21
    dd TRAP_HANDLER_0x22
    dd TRAP_HANDLER_0x23
    dd TRAP_HANDLER_0x24
    dd TRAP_HANDLER_0x25
    dd TRAP_HANDLER_0x26
    dd TRAP_HANDLER_0x27
    dd TRAP_HANDLER_0x28
    dd TRAP_HANDLER_0x29
    dd TRAP_HANDLER_0x2a
    dd TRAP_HANDLER_0x2b
    dd TRAP_HANDLER_0x2c
    dd TRAP_HANDLER_0x2d
    dd TRAP_HANDLER_0x2e
    dd TRAP_HANDLER_0x2f
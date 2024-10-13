[bits 32]

; multiboot2 header
magic   equ 0xe85250d6
i386    equ 0
lenght  equ header_end - header_start

section .multiboot2
header_start:
    dd magic
    dd i386
    dd lenght
    dd -(magic+i386+lenght) ; 校验和
    ; 结束标记
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

extern kernel_init
extern console_init
extern gdt_init
extern gdt_ptr

extern memory_init

code_selector equ (1 << 3)
data_selector equ (2 << 3)

section .text
global _start:
_start:
    ; 为了与后面可能使用的grub引导兼容
    push ebx; ards_count
    push eax; magic
    
    call console_init
    call gdt_init

    lgdt [gdt_ptr]; grub 启动需要尽早重设 gdt
    jmp dword code_selector:_gdt_refresh
_gdt_refresh:

    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    call memory_init

    mov esp, 0x10000

    push L6 ; return address for
    push kernel_init ; return to kernel
    ret
L6:
	jmp L6
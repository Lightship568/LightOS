[bits 32]

extern kernel_init
extern console_init
extern memory_init
global _start:
_start:
    ; 为了与后面可能使用的grub引导兼容
    push ebx; ards_count
    push eax; magic
    
    call console_init
    call memory_init

    push L6 ; return address for
    push kernel_init ; return to kernel
    ret
L6:
	jmp L6
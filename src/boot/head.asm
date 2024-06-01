[bits 32]

extern kernel_init

global _start:
_start:
    push L6 ; return address for
    push kernel_init ; return to kernel
    ret
L6:
	jmp L6
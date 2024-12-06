[bits 32]

section .text
global _start

_start:
    ;write(stdout, message, sizeof(message))
    mov eax, 4; write
    mov ebx, 1; stdout
    mov ecx, message
    mov edx, message.end - message - 1
    int 0x80

    ;exit(0)
    mov eax, 1; exit
    mov ebx, 0; code
    int 0x80

section .data

message:
    db "Hello Lightos!!", 10, 0
    .end:

section .bss

buffer: resb 1024; reserve 1024 bytes empty space


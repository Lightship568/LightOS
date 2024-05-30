[bits 32]

section .text; 代码段

global inb
inb:

    push ebp
    mov ebp, esp

    xor eax, eax
    mov edx, [ebp +8]; 函数参数第一个参数，也就是port
    in al, dx;  读取端口号[dx]的值并输入al

    ; 延迟等待读
    jmp $+2
    jmp $+2
    jmp $+2

    leave
    ret
    
global outb
outb:
    push ebp; 
    mov ebp, esp ; 保存帧

    mov edx, [ebp + 8]; port 
    mov eax, [ebp + 12]; value
    out dx, al; 将 al 中的 8 bit 输入出到 端口号 dx

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave ; 恢复栈帧
    ret

global inw
inw:
    push ebp; 
    mov ebp, esp ; 保存帧

    xor eax, eax ; 将 eax 清空
    mov edx, [ebp + 8]; port 
    in ax, dx; 将端口号 dx 的 16 bit 输入到 ax

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave ; 恢复栈帧
    ret

global outw
outw:
    push ebp; 
    mov ebp, esp ; 保存帧

    mov edx, [ebp + 8]; port 
    mov eax, [ebp + 12]; value
    out dx, ax; 将 ax 中的 16 bit 输入出到 端口号 dx

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave ; 恢复栈帧
    ret


global inl ; 将 inl 导出
inl:
    push ebp; 
    mov ebp, esp ; 保存帧

    xor eax, eax ; 将 eax 清空
    mov edx, [ebp + 8]; port 
    in eax, dx; 将端口号 dx 的 32 bit 输入到 eax

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave ; 恢复栈帧
    ret

global outl
outl:
    push ebp; 
    mov ebp, esp ; 保存帧

    mov edx, [ebp + 8]; port 
    mov eax, [ebp + 12]; value
    out dx, eax; 将 eax 中的 32 bit 输入出到 端口号 dx

    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟
    jmp $+2 ; 一点点延迟

    leave ; 恢复栈帧
    ret


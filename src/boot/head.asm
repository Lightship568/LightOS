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
    ; ; 入口地址
    dw 3    ; type entry address(manufacture 3.1.6)
    dw 0    ; flag
    dd 12   ; size
    dd 0x10040 ; entry address
    ; ; 8字节对齐
    times (8 - ($ - $$) % 8) db 0
    ; 结束标记
    dw 0    ; type
    dw 0    ; flags
    dd 8    ; size
header_end:

extern kernel_init
extern device_init
extern console_init
extern temp_gdt_init
extern gdt_init
extern gdt_ptr

extern mapping_init
extern memory_init
extern unset_low_mapping

code_selector equ (1 << 3)
data_selector equ (2 << 3)

section .text
global _start:
_start:
    ; 为了与后面可能使用的grub引导兼容
    ; 拷贝到0x29000，防止进入memory_init前被分页覆盖
    ; push ebx; ards_count
    push 0x30000; ards_count
    push eax; magic

    mov esi, ebx          ; ESI 指向源地址 (EBX指向的地址)
    mov edi, 0x30000      ; EDI 指向目标地址 (0x30000)
    mov ecx, 1024         ; ECX = 1024 个双字 (4096 字节 / 4 = 1024 个双字)
rep movsd                 ; 将 ESI 所指向的内存拷贝到 EDI（每次拷贝 4 字节）
                          ; ECX 用作计数器，执行 1024 次拷贝
    call temp_gdt_init ; grub 启动需要尽早重设 gdt，使用临时gdt(无全局变量)过度
    lgdt [0x29000]
    jmp dword code_selector:(_gdt_refresh_temp-0xC0000000)
    ; jmp dword code_selector:(_gdt_refresh_temp)
_gdt_refresh_temp:

    mov ax, data_selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax


    call mapping_init ; 页表配置，启动分页，跳转高地址，清空低地址PDE

    call gdt_init ; 更新基于全局变量的gdt
    lgdt [gdt_ptr]
    jmp dword code_selector:_gdt_refresh ; jmp刷新gdt并跳转到高地址
_gdt_refresh:

    call device_init

    call console_init

    call memory_init ; 获取和计算启动的内存参数

    mov esp, 0xC0010000

    call unset_low_mapping ; 清空内核在低地址的映射

    push L6 ; return address for
    push kernel_init ; return to kernel
    ret
L6:
	jmp L6
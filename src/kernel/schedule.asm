[bits 32]

section .text

global switch_to
switch_to:
    push ebp
    mov ebp, esp

    push ebx
    push esi
    push edi

    mov esp, [ebp + 8 + 4]
    mov dword [ebp + 8 + 12], back ; current eip
    mov eax, [ebp + 8]; next eip
    jmp eax

back:
    pop edi
    pop esi
    pop ebx
    pop ebp

    ret

; switch_to:
;     push ebp
;     mov ebp, esp

;     push ebx
;     push esi
;     push edi

;     mov eax, esp;
;     and eax, 0xfffff000; current

;     mov [eax], esp

;     mov eax, [ebp + 8]; next
;     mov esp, [eax]

;     pop edi
;     pop esi
;     pop ebx
;     pop ebp

;     ret

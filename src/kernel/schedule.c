#include <sys/global.h>
#include <lightos/task.h>

// void save_state(struct tss_t *tss, void *eip) {
//     asm volatile (
//         "mov %%esp, %0\n"        // 保存当前 esp 到 tss->esp
//         "mov %%ebp, %1\n"        // 保存当前 ebp 到 tss->ebp
//         : "=m"(tss->esp), "=m"(tss->ebp), "=m"(tss->eip)  // 输出约束
//         :                        // 输入约束为空
//         : "memory"               // 告诉编译器汇编代码会修改内存
//     );
//     tss->eip = (u32)eip;             // 手动保存 eip
// }

// void load_state(struct tss_t *tss) {
//     asm volatile (
//         "mov %0, %%esp\n"        // 恢复 esp
//         "mov %1, %%ebp\n"        // 恢复 ebp
//         "jmp *%2\n"              // 跳转到 eip
//         :                        // 输出约束为空
//         : "m"(tss->esp), "m"(tss->ebp), "m"(tss->eip)  // 输入约束
//         : "memory"               // 告诉编译器汇编代码会修改内存
//     );
// }

void save_state(struct tss_t* tss, void* eip) {
    asm volatile (
        "mov %%esp, %0\n"        // 保存 esp
        "mov %%ebp, %1\n"        // 保存 ebp
        : "=m"(tss->esp), "=m"(tss->ebp)  // 输出约束
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );
    tss->eip = (u32)eip;             // 手动保存 eip

    asm volatile (
        "mov %%eax, %0\n"        // 保存 eax
        "mov %%ebx, %1\n"        // 保存 ebx
        "mov %%ecx, %2\n"        // 保存 ecx
        "mov %%edx, %3\n"        // 保存 edx
        : "=r"(tss->eax), "=r"(tss->ebx), "=r"(tss->ecx), "=r"(tss->edx)
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    asm volatile (
        "mov %%esi, %0\n"        // 保存 esi
        "mov %%edi, %1\n"        // 保存 edi
        : "=r"(tss->esi), "=r"(tss->edi)
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    asm volatile (
        "pushf\n"                // 推送 eflags 到栈
        "pop %0\n"               // 保存 eflags
        : "=r"(tss->eflags)      // 输出约束
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );
}

void load_state(struct tss_t* tss) {
    asm volatile (
        "mov %0, %%eax\n"        // 恢复 eax
        "mov %1, %%ebx\n"        // 恢复 ebx
        "mov %2, %%ecx\n"        // 恢复 ecx
        "mov %3, %%edx\n"        // 恢复 edx
        :                        // 无输出寄存器
        : "r"(tss->eax), "r"(tss->ebx), "r"(tss->ecx), "r"(tss->edx)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "mov %0, %%esi\n"        // 恢复 esi
        "mov %1, %%edi\n"        // 恢复 edi
        :                        // 无输出寄存器
        : "r"(tss->esi), "r"(tss->edi)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "push %0\n"              // 恢复 eflags
        "popf\n"                 // 设置 eflags
        :                        // 无输出寄存器
        : "r"(tss->eflags)       // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "mov %0, %%esp\n"        // 恢复 esp
        "mov %1, %%ebp\n"        // 恢复 ebp
        :                        // 无输出寄存器
        : "m"(tss->esp), "m"(tss->ebp)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );

    // 跳转到保存的 eip，由于恢复了栈，因此局部变量tss丢失，需使用current
    asm volatile (
        "jmp *%0\n"
        :                        // 无输出寄存器
        : "m"(current->tss.eip)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    
}

#include <sys/global.h>

void save_state(struct tss_t *tss) {
    asm volatile (
        "mov %%esp, %0\n"        // 保存当前 esp 到 tss->esp
        "mov %%ebp, %1\n"        // 保存当前 ebp 到 tss->ebp
        "call 1f\n"              // 使用 call 来获取 eip
        "1: pop %2\n"            // 保存 eip 到 tss->eip
        : "=m"(tss->esp), "=m"(tss->ebp), "=m"(tss->eip)  // 输出约束
        :                        // 输入约束为空
        : "memory"               // 告诉编译器汇编代码会修改内存
    );
}

void load_state(struct tss_t *tss) {
    asm volatile (
        "mov %0, %%esp\n"        // 恢复 esp
        "mov %1, %%ebp\n"        // 恢复 ebp
        "jmp *%2\n"              // 跳转到 eip
        :                        // 输出约束为空
        : "m"(tss->esp), "m"(tss->ebp), "m"(tss->eip)  // 输入约束
        : "memory"               // 告诉编译器汇编代码会修改内存
    );
}

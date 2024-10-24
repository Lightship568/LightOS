#include <sys/global.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <lightos/memory.h>

/**
 * 软件实现上下文切换功能
 */

void switch_to(int n) {
    assert(n >= 0 && n < NR_TASKS && task_list[n] != NULL);

    if (current == task_list[n])
        return;
    
    current->tss.eip = (u32)&&resume_execution; // 手动保存 eip

    // 保存状态
    asm volatile (
        "mov %%esp, %0\n"        // 保存 esp
        "mov %%ebp, %1\n"        // 保存 ebp
        : "=m"(current->tss.esp), "=m"(current->tss.ebp)  // 输出约束
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    asm volatile (
        "mov %%eax, %0\n"        // 保存 eax
        "mov %%ebx, %1\n"        // 保存 ebx
        "mov %%ecx, %2\n"        // 保存 ecx
        "mov %%edx, %3\n"        // 保存 edx
        : "=r"(current->tss.eax), "=r"(current->tss.ebx), "=r"(current->tss.ecx), "=r"(current->tss.edx)
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    asm volatile (
        "mov %%esi, %0\n"        // 保存 esi
        "mov %%edi, %1\n"        // 保存 edi
        : "=r"(current->tss.esi), "=r"(current->tss.edi)
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    asm volatile (
        "pushf\n"                // 推送 eflags 到栈
        "pop %0\n"               // 保存 eflags
        : "=r"(current->tss.eflags)      // 输出约束
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );

    // 切换程序状态
    if (current->state == TASK_RUNNING){ // 运行状态，非阻塞等其他状态
        current->state = TASK_READY;
    }
    // 切换current
    current = task_list[n];
    
    assert(current->state == TASK_READY); //能够被调度的一定是 READY 的任务
    current->state = TASK_RUNNING;
    
    // 切换cr3
    set_cr3(current->pde);

    // 切换tdr指向的tss.esp0，使得ring3中断可以进入正确的内核栈
    tss.esp0 = current->tss.esp0;

    // 加载状态
    asm volatile (
        "mov %0, %%eax\n"        // 恢复 eax
        "mov %1, %%ebx\n"        // 恢复 ebx
        "mov %2, %%ecx\n"        // 恢复 ecx
        "mov %3, %%edx\n"        // 恢复 edx
        :                        // 无输出寄存器
        : "r"(current->tss.eax), "r"(current->tss.ebx), "r"(current->tss.ecx), "r"(current->tss.edx)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "mov %0, %%esi\n"        // 恢复 esi
        "mov %1, %%edi\n"        // 恢复 edi
        :                        // 无输出寄存器
        : "r"(current->tss.esi), "r"(current->tss.edi)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "push %0\n"              // 恢复 eflags
        "popf\n"                 // 设置 eflags
        :                        // 无输出寄存器
        : "r"(current->tss.eflags)       // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    asm volatile (
        "mov %0, %%esp\n"        // 恢复 esp
        "mov %1, %%ebp\n"        // 恢复 ebp
        :                        // 无输出寄存器
        : "m"(current->tss.esp), "m"(current->tss.ebp)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
    // 跳转到保存的 eip，此时已经恢复了栈
    asm volatile (
        "jmp *%0\n"
        :                        // 无输出寄存器
        : "m"(current->tss.eip)  // 输入约束
        : "memory"               // 汇编可能修改内存
    );
resume_execution:
    // 从这里恢复执行
}

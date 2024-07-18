#include <lightos/interrupt.h>
#include <lightos/syscall.h>
#include <sys/assert.h>
#include <sys/types.h>
#include <lib/debug.h>
#include <lightos/task.h>

u32 nr_syscall = NR_SYSCALL;
handler_t syscall_table[NR_SYSCALL];

static _inline u32 _syscall0(u32 nr){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr)
        : "memory");
    return ret;
}

// 内核的系统调用封装
void yield(void){
    _syscall0(SYS_NR_YIELD);
}

// 默认系统调用处理，返回-1
u32 sys_default(u32 vector, u32 edi, u32 esi, u32 ebp, u32 esp, u32 ebx,u32 edx, u32 ecx, u32 eax){
    DEBUGK("Haven't implement syscall 0x%x\n", eax);
    return -1;
}

// 系统调用表初始化，把散落各处的sys_处理函数指针保存到表中
void syscall_init(void){
    for (size_t i = 0; i < NR_SYSCALL; ++i){
        syscall_table[i] = sys_default;
    }
    syscall_table[SYS_NR_YIELD] = sys_yield;
}

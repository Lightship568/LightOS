#include <sys/types.h>
#include <lib/syscall.h>
#include <lightos/interrupt.h>
#include <lightos/task.h>
#include <lib/debug.h>
#include <lightos/console.h>
#include <lightos/memory.h>

u32 nr_syscall = NR_SYSCALL;
handler_t syscall_table[NR_SYSCALL];

// 默认系统调用处理，返回-1
u32 sys_default(u32 vector, u32 edi, u32 esi, u32 ebp, u32 esp, u32 ebx,u32 edx, u32 ecx, u32 eax){
    DEBUGK("Haven't implement syscall 0x%x\n", eax);
    return -1;
}

u32 sys_write(fd_t fd, char* buf, u32 len){
    if (fd == stdout){
        return console_write(buf, len);

    }
    return -1;
}

extern time_t sys_time(); // clock.c
extern void sys_test(); // anywhere

// 系统调用表初始化，把散落各处的sys_处理函数指针保存到表中
void syscall_init(void){
    for (size_t i = 0; i < NR_SYSCALL; ++i){
        syscall_table[i] = sys_default;
    }
    syscall_table[SYS_NR_TEST]      = sys_test;
    syscall_table[SYS_NR_YIELD]     = sys_yield;
    syscall_table[SYS_NR_SLEEP]     = sys_sleep;
    syscall_table[SYS_NR_WRITE]     = sys_write;
    syscall_table[SYS_NR_BRK]       = sys_brk;
    syscall_table[SYS_NR_GETPID]    = sys_getpid;
    syscall_table[SYS_NR_GETPPID]   = sys_getppid;
    syscall_table[SYS_NR_FORK]      = sys_fork;
    syscall_table[SYS_NR_EXIT]      = sys_exit;
    syscall_table[SYS_NR_WAITPID]   = sys_waitpid;
    syscall_table[SYS_NR_TIME]      = sys_time;

    DEBUGK("Syscall initialized\n");
}

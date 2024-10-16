#include <lib/syscall.h>
#include <sys/assert.h>
#include <lightos/task.h>

// ebx，ecx，edx，esi，edi
// intel 不采用栈传递系统调用参数

static _inline u32 _syscall0(u32 nr){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr)
        : "memory");
    return ret;
}

static _inline u32 _syscall1(u32 nr, u32 arg1){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1)
        : "memory");
    return ret;
}
static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2)
        : "memory");
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3){
    u32 ret;
    asm volatile(
        "int $0x80\n"
        : "=a"(ret)
        : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3)
        : "memory");
    return ret;
}

// 内核的系统调用封装
void yield(void){
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms){
    _syscall1(SYS_NR_SLEEP, ms);
}

int32 write(fd_t fd, char *buf, u32 len){
    _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}
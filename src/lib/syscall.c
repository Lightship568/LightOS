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
    return _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

int32 brk(void* addr){
    return _syscall1(SYS_NR_BRK, (u32)addr);
}

pid_t getpid(){
    return _syscall0(SYS_NR_GETPID);
}

pid_t getppid(){
    return _syscall0(SYS_NR_GETPPID);
}

pid_t fork(){
    return _syscall0(SYS_NR_FORK);
}

void exit(u32 status){
    _syscall1(SYS_NR_EXIT, (u32)status);
}

pid_t waitpid(pid_t pid, int32* status, int32 options){
    return _syscall3(SYS_NR_WAITPID, (u32)pid, (u32)status, (u32)options);
}

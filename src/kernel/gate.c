#include <lib/debug.h>
#include <lib/print.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <lightos/cache.h>
#include <lightos/console.h>
#include <lightos/device.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/types.h>
#include <lightos/fs.h>

u32 nr_syscall = NR_SYSCALL;
handler_t syscall_table[NR_SYSCALL];

// 默认系统调用处理，返回-1
u32 sys_default(u32 vector,
                u32 edi,
                u32 esi,
                u32 ebp,
                u32 esp,
                u32 ebx,
                u32 edx,
                u32 ecx,
                u32 eax) {
    DEBUGK("Haven't implement syscall 0x%x\n", eax);
    return -1;
}

static u32 sys_test(void) {
    char ch;
    device_t *device;

    device_t* serial = device_find(DEV_SERIAL, 0);
    assert(serial);

    device_t* keyboard = device_find(DEV_KEYBOARD, 0);
    assert(serial);

    device_t* console = device_find(DEV_CONSOLE, 0);
    assert(serial);

    device_read(serial->dev, &ch, 1, 0, 0);
    // device_read(keyboard->dev, &ch, 1, 0, 0);

    device_write(serial->dev, &ch, 1, 0, 0);
    device_write(console->dev, &ch, 1, 0, 0);

    
    return 255;
}

extern time_t sys_time();  // clock.c
extern mode_t sys_umask(mode_t mask); // system.c
extern int32 sys_execve(char* filename, char*argv[],char* envp[]); // execve.c
extern int32 sys_ioctl(fd_t fd, int cmd, void* args); // ioctl.c
extern int32 sys_stty(void); // tty.c
extern int32 sys_gtty(void); // tty.c
extern int32 sys_kill(pid_t pid); // kernel/signal.c
extern int sys_sgetmask(void);
extern int sys_ssetmask(int newmask);
extern int sys_signal(int sig, int handler, int restorer);
extern int sys_sigaction(int sig, sigaction_t* action, sigaction_t* oldaction);

// 系统调用表初始化，把散落各处的sys_处理函数指针保存到表中
void syscall_init(void) {
    for (size_t i = 0; i < NR_SYSCALL; ++i) {
        syscall_table[i] = sys_default;
    }
    syscall_table[SYS_NR_TEST] = sys_test;
    syscall_table[SYS_NR_YIELD] = sys_yield;
    syscall_table[SYS_NR_SLEEP] = sys_sleep;
    syscall_table[SYS_NR_READ] = sys_read;
    syscall_table[SYS_NR_WRITE] = sys_write;
    syscall_table[SYS_NR_BRK] = sys_brk;
    syscall_table[SYS_NR_GETPID] = sys_getpid;
    syscall_table[SYS_NR_GETPPID] = sys_getppid;
    syscall_table[SYS_NR_FORK] = sys_fork;
    syscall_table[SYS_NR_EXIT] = sys_exit;
    syscall_table[SYS_NR_WAITPID] = sys_waitpid;
    syscall_table[SYS_NR_TIME] = sys_time;
    syscall_table[SYS_NR_UMASK] = sys_umask;
    syscall_table[SYS_NR_MKDIR] = sys_mkdir;
    syscall_table[SYS_NR_RMDIR] = sys_rmdir;
    syscall_table[SYS_NR_LINK] = sys_link;
    syscall_table[SYS_NR_UNLINK] = sys_unlink;
    syscall_table[SYS_NR_OPEN] = sys_open;
    syscall_table[SYS_NR_CREAT] = sys_creat;
    syscall_table[SYS_NR_CLOSE] = sys_close;
    syscall_table[SYS_NR_LSEEK] = sys_lseek;
    syscall_table[SYS_NR_GETCWD] = sys_getcwd;
    syscall_table[SYS_NR_CHDIR] = sys_chdir;
    syscall_table[SYS_NR_CHROOT] = sys_chroot;
    syscall_table[SYS_NR_READDIR] = sys_readdir;
    syscall_table[SYS_NR_STAT] = sys_stat;
    syscall_table[SYS_NR_FSTAT] = sys_fstat;
    syscall_table[SYS_NR_MKNOD] = sys_mknod;
    syscall_table[SYS_NR_MOUNT] = sys_mount;
    syscall_table[SYS_NR_UMOUNT] = sys_umount;
    syscall_table[SYS_NR_MKFS] = sys_mkfs;
    syscall_table[SYS_NR_MMAP] = sys_mmap;
    syscall_table[SYS_NR_MUNMAP] = sys_munmap;
    syscall_table[SYS_NR_EXECVE] = sys_execve;
    syscall_table[SYS_NR_DUP] = sys_dup;
    syscall_table[SYS_NR_DUP2] = sys_dup2;
    syscall_table[SYS_NR_PIPE] = sys_pipe;
    syscall_table[SYS_NR_SETSID] = sys_setsid;
    syscall_table[SYS_NR_SETPGID] = sys_setpgid;
    syscall_table[SYS_NR_GETPGRP] = sys_getpgrp;
    syscall_table[SYS_NR_IOCTL] = sys_ioctl;
    syscall_table[SYS_NR_STTY] = sys_stty;
    syscall_table[SYS_NR_GTTY] = sys_gtty;
    syscall_table[SYS_NR_KILL] = sys_kill;
    syscall_table[SYS_NR_SIGNAL] = sys_signal;
    syscall_table[SYS_NR_SGETMASK] = sys_sgetmask;
    syscall_table[SYS_NR_SSETMASK] = sys_ssetmask;
    syscall_table[SYS_NR_SIGACTION] = sys_sigaction;



    DEBUGK("Syscall initialized\n");
}

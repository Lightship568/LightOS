#ifndef LIGHTOS_SYSCALL_H
#define LIGHTOS_SYSCALL_H

#include <lightos/stat.h>
#include <sys/types.h>

#define NR_SYSCALL 256

typedef enum syscall_t {
    SYS_NR_TEST,
    SYS_NR_EXIT = 1,
    SYS_NR_FORK = 2,
    SYS_NR_READ = 3,
    SYS_NR_WRITE = 4,
    SYS_NR_OPEN = 5,
    SYS_NR_CLOSE = 6,
    SYS_NR_WAITPID = 7,
    SYS_NR_CREAT = 8,
    SYS_NR_LINK = 9,
    SYS_NR_UNLINK = 10,
    SYS_NR_EXECVE = 11,
    SYS_NR_CHDIR = 12,
    SYS_NR_TIME = 13,
    SYS_NR_MKNOD = 14,
    SYS_NR_STAT = 18,
    SYS_NR_LSEEK = 19,
    SYS_NR_GETPID = 20,
    SYS_NR_MOUNT = 21,
    SYS_NR_UMOUNT = 22,
    SYS_NR_ALARM = 27,
    SYS_NR_FSTAT = 28,
    SYS_NR_STTY = 31,
    SYS_NR_GTTY = 32,
    SYS_NR_KILL = 37,
    SYS_NR_MKDIR = 39,
    SYS_NR_RMDIR = 40,
    SYS_NR_DUP = 41,
    SYS_NR_PIPE = 42,
    SYS_NR_BRK = 45,
    SYS_NR_SIGNAL = 48,
    SYS_NR_IOCTL = 54,
    SYS_NR_SETPGID = 57,
    SYS_NR_UNAME = 59,
    SYS_NR_UMASK = 60,
    SYS_NR_CHROOT = 61,
    SYS_NR_DUP2 = 63,
    SYS_NR_GETPPID = 64,
    SYS_NR_GETPGRP = 65,
    SYS_NR_SETSID = 66,
    SYS_NR_SIGACTION = 67,
    SYS_NR_SGETMASK = 68,
    SYS_NR_SSETMASK = 69,
    SYS_NR_READDIR = 89,
    SYS_NR_MMAP = 90,
    SYS_NR_MUNMAP = 91,
    SYS_NR_YIELD = 158,
    SYS_NR_SLEEP = 162,
    SYS_NR_GETCWD = 183,

    SYS_NR_SOCKET = 359,
    SYS_NR_BIND = 361,
    SYS_NR_CONNECT,
    SYS_NR_LISTEN,
    SYS_NR_ACCEPT,
    SYS_NR_GETSOCKOPT,
    SYS_NR_SETSOCKOPT,
    SYS_NR_GETSOCKNAME,
    SYS_NR_GETPEERNAME,
    SYS_NR_SENDTO,
    SYS_NR_SENDMSG,
    SYS_NR_RECVFROM,
    SYS_NR_RECVMSG,
    SYS_NR_SHUTDOWN,
    SYS_NR_RESOLV,

    SYS_NR_MKFS = NR_SYSCALL - 1,
} syscall_t;

enum mmap_type_t {
    PROT_NONE = 0,
    PROT_READ = 1,
    PROT_WRITE = 2,
    PROT_EXEC = 4,

    MAP_SHARED = 1,
    MAP_PRIVATE = 2,
    MAP_FIXED = 0x10,
};

typedef struct mmap_args {
    void* addr;
    size_t length;
    int prot;
    int flags;
    int fd;
    off_t offset;
} mmap_args;

void test(void);
void yield(void);
void sleep(u32 ms);
int32 read(fd_t fd, char* buf, u32 count);
int32 write(fd_t fd, char* buf, u32 count);
int32 brk(void* addr);
pid_t getpid();
pid_t getppid();
pid_t fork();
void exit(u32 status);
pid_t waitpid(pid_t pid, int32* status, int32 options);
time_t time();  // return seconds from 1970.01.01
mode_t umask(mode_t mask);
int mkdir(char* pathname, int mode);
int rmdir(char* pathname);
int link(char* oldname, char* newname);
int unlink(char* filename);
fd_t open(char* filename, int flags, int mode);
fd_t creat(char* filename, int mode);
void close(fd_t fd);
int lseek(fd_t, off_t offset, int whence);
int getcwd(char* buf, size_t size);
int chdir(char* pathname);
int chroot(char* pathname);
int readdir(fd_t fd, void* dir, int count);
int stat(char* filename, stat_t* statbuf);
int fstat(fd_t fd, stat_t* statbuf);
int mknod(char* filename, int mode, int dev);
int mount(char* devname, char* dirname, int flags);
int umount(char* target);
int mkfs(char* devname, int icount);
void* mmap(void* addr,
           size_t length,
           int prot,
           int flags,
           int fd,
           off_t offset);
int munmap(void* addr, size_t length);
int execve(char* filename, char*argv[],char* envp[]);
fd_t dup(fd_t oldfd);
fd_t dup2(fd_t oldfd, fd_t newfd);
int pipe(fd_t pipefd[2]);
pid_t setsid(void);
int setpgid(pid_t pid, pid_t pgid);
int setpgrp(void);
pid_t getpgrp(void);
int stty(void);
int gtty(void);
int ioctl(fd_t fd, int cmd, int args);
int kill(pid_t pid, int signal);


#endif
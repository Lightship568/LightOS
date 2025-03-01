#include <lib/syscall.h>
#include <lib/signal.h>

// ebx，ecx，edx，esi，edi
// intel 不采用栈传递系统调用参数

static _inline u32 _syscall0(u32 nr) {
    u32 ret;
    asm volatile("int $0x80\n" : "=a"(ret) : "a"(nr) : "memory");
    return ret;
}

static _inline u32 _syscall1(u32 nr, u32 arg1) {
    u32 ret;
    asm volatile("int $0x80\n" : "=a"(ret) : "a"(nr), "b"(arg1) : "memory");
    return ret;
}
static _inline u32 _syscall2(u32 nr, u32 arg1, u32 arg2) {
    u32 ret;
    asm volatile("int $0x80\n"
                 : "=a"(ret)
                 : "a"(nr), "b"(arg1), "c"(arg2)
                 : "memory");
    return ret;
}

static _inline u32 _syscall3(u32 nr, u32 arg1, u32 arg2, u32 arg3) {
    u32 ret;
    asm volatile("int $0x80\n"
                 : "=a"(ret)
                 : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3)
                 : "memory");
    return ret;
}

static _inline u32 _syscall4(u32 nr, u32 arg1, u32 arg2, u32 arg3, u32 arg4) {
    u32 ret;
    asm volatile("int $0x80\n"
                 : "=a"(ret)
                 : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4)
                 : "memory");
    return ret;
}

static _inline u32 _syscall5(u32 nr,
                             u32 arg1,
                             u32 arg2,
                             u32 arg3,
                             u32 arg4,
                             u32 arg5) {
    u32 ret;
    asm volatile("int $0x80\n"
                 : "=a"(ret)
                 : "a"(nr), "b"(arg1), "c"(arg2), "d"(arg3), "S"(arg4),
                   "D"(arg5)
                 : "memory");
    return ret;
}

// 内核的系统调用封装
void test(void) {
    _syscall0(SYS_NR_TEST);
}

void yield(void) {
    _syscall0(SYS_NR_YIELD);
}

void sleep(u32 ms) {
    _syscall1(SYS_NR_SLEEP, ms);
}

int32 read(fd_t fd, char* buf, u32 len) {
    return _syscall3(SYS_NR_READ, fd, (u32)buf, len);
}

int32 write(fd_t fd, char* buf, u32 len) {
    return _syscall3(SYS_NR_WRITE, fd, (u32)buf, len);
}

int32 brk(void* addr) {
    return _syscall1(SYS_NR_BRK, (u32)addr);
}

pid_t getpid() {
    return _syscall0(SYS_NR_GETPID);
}

pid_t getppid() {
    return _syscall0(SYS_NR_GETPPID);
}

pid_t fork() {
    return _syscall0(SYS_NR_FORK);
}

void exit(u32 status) {
    _syscall1(SYS_NR_EXIT, (u32)status);
}

pid_t waitpid(pid_t pid, int32* status, int32 options) {
    return _syscall3(SYS_NR_WAITPID, (u32)pid, (u32)status, (u32)options);
}

time_t time() {
    return _syscall0(SYS_NR_TIME);
}

mode_t umask(mode_t mask) {
    return _syscall1(SYS_NR_UMASK, (u32)mask);
}

int mkdir(char* pathname, int mode) {
    return _syscall2(SYS_NR_MKDIR, (u32)pathname, (u32)mode);
}

int rmdir(char* pathname) {
    return _syscall1(SYS_NR_RMDIR, (u32)pathname);
}

int link(char* oldname, char* newname) {
    return _syscall2(SYS_NR_LINK, (u32)oldname, (u32)newname);
}

int unlink(char* filename) {
    return _syscall1(SYS_NR_UNLINK, (u32)filename);
}

fd_t open(char* filename, int flags, int mode) {
    return _syscall3(SYS_NR_OPEN, (u32)filename, (u32)flags, (u32)mode);
}

fd_t creat(char* filename, int mode) {
    return _syscall2(SYS_NR_CREAT, (u32)filename, (u32)mode);
}

void close(fd_t fd) {
    _syscall1(SYS_NR_CLOSE, (u32)fd);
}

int lseek(fd_t fd, off_t offset, int whence) {
    return _syscall3(SYS_NR_LSEEK, (u32)fd, (u32)offset, (u32)whence);
}

int getcwd(char* buf, size_t size) {
    return _syscall2(SYS_NR_GETCWD, (u32)buf, (u32)size);
}

int chdir(char* pathname) {
    return _syscall1(SYS_NR_CHDIR, (u32)pathname);
}

int chroot(char* pathname) {
    return _syscall1(SYS_NR_CHROOT, (u32)pathname);
}

int readdir(fd_t fd, void* dir, int count) {
    return _syscall3(SYS_NR_READDIR, (u32)fd, (u32)dir, (u32)count);
}

int stat(char* filename, stat_t* statbuf) {
    return _syscall2(SYS_NR_STAT, (u32)filename, (u32)statbuf);
}

int fstat(fd_t fd, stat_t* statbuf) {
    return _syscall2(SYS_NR_FSTAT, (u32)fd, (u32)statbuf);
}

int mknod(char* filename, int mode, int dev) {
    return _syscall3(SYS_NR_MKNOD, (u32)filename, (u32)mode, (u32)dev);
}

int mount(char* devname, char* dirname, int flags) {
    return _syscall3(SYS_NR_MOUNT, (u32)devname, (u32)dirname, (u32)flags);
}

int umount(char* target) {
    return _syscall1(SYS_NR_UMOUNT, (u32)target);
}

int mkfs(char* devname, int icount) {
    return _syscall2(SYS_NR_MKFS, (u32)devname, (u32)icount);
}

void* mmap(void* addr,
           size_t length,
           int prot,
           int flags,
           int fd,
           off_t offset) {
    mmap_args args;
    args.addr = addr;
    args.length = length;
    args.prot = prot;
    args.flags = flags;
    args.fd = fd;
    args.offset = offset;
    return (void*)_syscall1(SYS_NR_MMAP, (u32)&args);
}

int munmap(void* addr, size_t length) {
    return _syscall2(SYS_NR_MUNMAP, (u32)addr, (u32)length);
}

int execve(char* filename, char* argv[], char* envp[]) {
    return _syscall3(SYS_NR_EXECVE, (u32)filename, (u32)argv, (u32)envp);
}

fd_t dup(fd_t oldfd) {
    return _syscall1(SYS_NR_DUP, (u32)oldfd);
}

fd_t dup2(fd_t oldfd, fd_t newfd) {
    return _syscall2(SYS_NR_DUP2, (u32)oldfd, (u32)newfd);
}

int pipe(fd_t pipefd[2]) {
    return _syscall1(SYS_NR_PIPE, (u32)pipefd);
}

pid_t setsid(void) {
    return _syscall0(SYS_NR_SETSID);
}

int setpgid(pid_t pid, pid_t pgid) {
    return _syscall2(SYS_NR_SETPGID, (u32)pid, (u32)pgid);
}

int setpgrp(void) {
    return setpgid(0, 0);
}

pid_t getpgrp(void) {
    return _syscall0(SYS_NR_GETPGRP);
}

int stty(void) {
    return _syscall0(SYS_NR_STTY);
}

int gtty(void) {
    return _syscall0(SYS_NR_GTTY);
}

int ioctl(fd_t fd, int cmd, int args) {
    return _syscall3(SYS_NR_IOCTL, (u32)fd, (u32)cmd, (u32)args);
}

int kill(pid_t pid, int signal) {
    return _syscall2(SYS_NR_KILL, (u32)pid, (u32)signal);
}

int sgetmask(void){
    return _syscall0(SYS_NR_SGETMASK);
}

int ssetmask(int newmask){
    return _syscall1(SYS_NR_SSETMASK, (u32)newmask);
}

extern int restorer(void);
int signal(int sig, int handler){
    return _syscall3(SYS_NR_SIGNAL, (u32)sig, (u32)handler, (u32)restorer);
}

int sigaction(int sig, sigaction_t* action, sigaction_t* oldaction){
    return _syscall3(SYS_NR_SIGACTION, (u32)sig, (u32)action, (u32)oldaction);
}

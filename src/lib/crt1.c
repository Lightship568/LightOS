#include <lib/string.h>
#include <lib/syscall.h>
#include <sys/types.h>

int main(int argc, char** argv, char** envp);

// libc 构造函数
weak void _init() {}

// libc 析构函数
weak void _fini() {}

int __libc_start_main(int (*main)(int argc, char** argv, char** envp),
                      int argc,
                      char** argv,
                      void (*_init)(),
                      void (*_fini)(),
                      void (*ldso)(),  // 动态连接器
                      void* stack_end) {
    // argv[argc] 是 NULL，因此 envp = argv + argc + 1 指向环境变量的开始位置。
    char** envp = argv + argc + 1; 
    _init();
    int i = main(argc, argv, envp);
    _fini();
    exit(i);
}

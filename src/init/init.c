#include <lib/arena.h>
#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/print.h>
#include <lib/string.h>
#include <lib/syscall.h>
#include <lightos/fs.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/global.h>
#include <sys/types.h>

void init_uthread(void);

void move_to_user_mode(void) {
    // 将 init_kthread 进程通过 iret 返回到 Ring3 执行

    intr_frame_t
        __stack_clear;  // 添加一个局部变量使得下面的iframe指针不会被后续操作覆盖
    intr_frame_t* iframe =
        (intr_frame_t*)((u32)task_list[1] + PAGE_SIZE - sizeof(intr_frame_t));
    task_t* task = get_current();
    /**
     * vmap是用户空间的实际管理者
     * 一页vmap->buf可以管理8*4k*4k=128M即0x08000000
     * 若想要完全利用前3G的虚拟地址，则需要分配3G/128M=24个页作为buf
     * 现代linux采用mm_struct中的红黑树VMA分段进行管理
     * 位图管理的方式空间时间效率太低、且没有权限标注，是古老的管理方式
     * 因此目前仅用一个页的大小做测试（单进程最大虚拟地址128M）
     *  */
    task->vmap = kmalloc(sizeof(bitmap_t));
    void* buf = (void*)alloc_kpage(1);
    bitmap_init(task->vmap, buf, PAGE_SIZE, 0);

    // 切换为独立页表（init自己拷贝自己），此时无用户态页，本质就是内核地址共享
    copy_pde(task);
    set_cr3(task->pde);

    // 切换 uid 为用户进程
    task->uid = USER_RING3;
    // init是所有进程的父进程
    task->ppid = task->pid;

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = USER_STACK_TOP - 1;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR;  // 当前的GDT配置下，用户可以修改内核内存。

    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = LIGHTOS_MAGIC;

    iframe->eip =
        (u32)init_uthread;  // 当前的 GDT 中 USER_CODE 是可以执行内核代码的
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9);  // IOPL=0, 固定1, IF中断开启
    // iframe->esp = ((u32)alloc_kpage(1) + PAGE_SIZE);
    // //暂时用一个内核页放用户栈
    iframe->esp = USER_STACK_TOP;

    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe));
}

extern int printf(const char* fmt, ...);
extern void lsh_main(void);

void init_uthread(void) {
    while (true) {
        u32 status;
        pid_t pid = fork();
        if (pid){
            pid_t child = waitpid(pid, &status, -1);
            printf("wait pid %d status %d %d\n", child, status, time());
        }else{
            lsh_main();
        }
    }
}

void init_kthread(void) {
    char str[11] = {0};
    // printk("trying to read from keyboard\n");
    // keyboard_read(NULL, str, 1);
    // printk("read %d: %s\n", 1, str);

    char* p = kmalloc(0x20);
    memset(p, 'a', 0x20);
    p[0x1F] = '\0';
    printk("Test kmalloc: %s\n", p);
    kfree(p);

    printk("kernel address is 0x%p\n", &&high_check);
high_check:

    DEBUGK("init kthread move to user mode\n");

    move_to_user_mode();
}
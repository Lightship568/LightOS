#include <sys/types.h>
#include <sys/global.h>
#include <lightos/task.h>
#include <lib/syscall.h>
#include <lib/debug.h>
#include <lightos/memory.h>
#include <lib/arena.h>
#include <lib/string.h>
#include <lib/print.h>

void init_uthread(void);

void move_to_user_mode(void){
    // 将 init_kthread 进程通过 iret 返回到 Ring3 执行

    intr_frame_t __stack_clear; //添加一个局部变量使得下面的iframe指针不会被后续操作覆盖
    intr_frame_t *iframe = (intr_frame_t *)((u32)task_list[1] + PAGE_SIZE - sizeof(intr_frame_t));

    iframe->vector = 0x20;
    iframe->edi = 1;
    iframe->esi = 2;
    iframe->ebp = 3;
    iframe->esp_dummy = 4;
    iframe->ebx = 5;
    iframe->edx = 6;
    iframe->ecx = 7;
    iframe->eax = 8;

    iframe->gs = 0;
    iframe->ds = USER_DATA_SELECTOR;
    iframe->es = USER_DATA_SELECTOR;
    iframe->fs = USER_DATA_SELECTOR;
    iframe->ss = USER_DATA_SELECTOR; // 当前的GDT配置下，用户可以修改内核内存。

    iframe->cs = USER_CODE_SELECTOR;

    iframe->error = LIGHTOS_MAGIC;

    iframe->eip = (u32)init_uthread; // 当前的 GDT 中 USER_CODE 是可以执行内核代码的
    iframe->eflags = (0 << 12 | 0b10 | 1 << 9); //IOPL=0, 固定1, IF中断开启
    iframe->esp = ((u32)alloc_kpage(1) + PAGE_SIZE); //暂时用一个内核页放用户栈

    asm volatile(
        "movl %0, %%esp\n"
        "jmp interrupt_exit\n" ::"m"(iframe)
    );

}

extern int printf(const char *fmt, ...);

void init_uthread(void){
    int cnt = 0;
    int ret = 0;
    while (true){
        // ret = printf("init thread in user mode, times %d\n", cnt++);
        // printf("last printf output %d char\n", ret);
        // sleep(1000);
    }
}


void init_kthread(void){
    char str[11] = {0};
    // printk("trying to read from keyboard\n");
    // keyboard_read(str, 1);
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
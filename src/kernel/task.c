#include <lib/debug.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>
#include <lib/list.h>
#include <lightos/syscall.h>

task_t* task_list[NR_TASKS] = {0};
task_t* current = (task_t*)NULL;

task_t* get_current() {
    return current;
}

// 分配一个内核页用于存放 pcb 与 内核栈，返回 pid，若达到 NR_TASKS 则返回 -1
static pid_t get_free_task() {
    for (int i = 0; i < NR_TASKS; ++i) {
        if (task_list[i] == NULL) {
            task_list[i] = (task_t*)alloc_kpage(1);
            return i;
        }
    }
    panic("Task count reach top limit of %d, can't create a new task!\n",
          NR_TASKS);
    return -1;
}

// 创建新task，返回pid，失败则返回-1
pid_t kfork() {
    pid_t pid = get_free_task();

    return pid;
}

/**
 * 软件实现上下文切换功能
 */

extern void save_state(struct tss_t *tss);
extern void load_state(struct tss_t *tss);

void switch_to(int n) {
    assert(n >= 0 && n < NR_TASKS && task_list[n] != NULL);
    if (current == task_list[n])
        return;

    /**
     * 系统初始化的内核栈与新分配的两个进程不一致(0x10000和alloc_kpage的0x11000)
     * 但是是通过进程调用来开始执行第一个进程的，因此导致目前的栈与进程A并不相符
     * 只能暂时性的将save删掉了，目前没有办法从yield或者抢占处继续执行该进程。
     * 等到move_to_user搞定了，就可以打开注释了
     */

    // save_state(&current->tss);
    current = task_list[n];
    load_state(&current->tss);

    // 跳转到新任务的 EIP
    // asm volatile("jmp *%0\n" : : "m"(current->tss.eip));
}

void schedule() {
    // 获取 current 的 pid
    pid_t n = current->pid;
    n += 1;
    for (int i = 0; i < NR_TASKS; ++i, ++n) {
        n %= NR_TASKS;
        if (task_list[n] != NULL && task_list[n]->ticks &&
            task_list[n]->state == TASK_READY) {
            switch_to(n);
        }
    }
    switch_to(0);
    // panic("NO READY TASK??\n");
}
#define INTERVEL 10000000

void task1() { //73402
    start_interrupt();
    while (true) {
        for(int j = 0; j < 5; ++j){
            printk("A");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

void task2() { //73453
    start_interrupt();
    while (true) {
        for(int j = 0; j < 5; ++j){
            printk("B");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

// 寻找 task_list 空闲

// 初始化进程pcb（包括tss）
pid_t task_create(void (*task_ptr)(void),
                 const char* name,
                 u32 priority,
                 u32 uid) {
    pid_t pid = get_free_task();
    task_t* task = task_list[pid];
    memset(task, 0, PAGE_SIZE);
    task->pid = pid;
    // todo stack 要设置一些中断进入内核栈的压入值？

    // todo wstrcpy name
    assert(sizeof(name) <= 16);
    strcpy(task->name, name);

    u32 stack = (u32)task + PAGE_SIZE;
    task->stack = (u32*)stack;
    task->jiffies = 0;
    task->ticks = priority;
    task->priority = priority;
    task->uid = uid;
    task->magic = LIGHTOS_MAGIC;

    task->tss.eip = (u32)task_ptr;
    task->tss.ebp = stack;
    task->tss.esp = stack;

    task->state = TASK_READY;

    return pid;
}

void task_test() {
    pid_t pid;
    pid = task_create(task1, "testA", 50, KERNEL_USER);
    pid = task_create(task2, "testB", 50, KERNEL_USER);
    current = task_list[pid];
    task2();
}

void sys_yield(void){
    schedule();
}

void sys_block(task_t *task, list_t *blist, task_state_t state){
    

}
void sys_unblock(task_t *task){

}
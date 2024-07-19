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

#define task1_base 0x1000
#define task2_base 0x2000

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

// todo 实现硬件任务切换（注册tss到gdt中实现硬件切换）
// extern descriptor_t gdt[GDT_SIZE];

/**
 * 下面先实现手动切换tss功能
 */
void save_state(tss_t* tss) {
    u32 eflags, eip;
    asm volatile(
        "movl %%esp, %0\n"
        "movl %%ebp, %1\n"
        "pushfl\n"
        "popl %2\n"
        "movl %%eax, %3\n"
        "movl %%ebx, %4\n"
        "call 1f\n"     // 调用标签1，保存EIP
        "1: popl %5\n"  // 将返回地址弹出到EIP变量中
        : "=m"(tss->esp), "=m"(tss->ebp), "=r"(eflags), "=m"(tss->eax),
          "=m"(tss->ebx), "=r"(eip)
        :
        : "memory");
    tss->eflags = eflags;
    tss->eip = eip;

    asm volatile(
        "movl %%ecx, %0\n"
        "movl %%edx, %1\n"
        "movl %%esi, %2\n"
        "movl %%edi, %3\n"
        : "=m"(tss->ecx), "=m"(tss->edx), "=m"(tss->esi), "=m"(tss->edi)
        :
        : "memory");
}

// 加载新任务状态
void load_state(tss_t* tss) {
    asm volatile(
        "movl %0, %%esp\n"
        "movl %1, %%ebp\n"
        "pushl %2\n"
        "popfl\n"
        "movl %3, %%eax\n"
        "movl %4, %%ebx\n"
        :
        : "m"(tss->esp), "m"(tss->ebp), "r"(tss->eflags), "m"(tss->eax),
          "m"(tss->ebx)
        : "memory");
    asm volatile(
        "movl %0, %%ecx\n"
        "movl %1, %%edx\n"
        "movl %2, %%esi\n"
        "movl %3, %%edi\n"
        :
        : "m"(tss->ecx), "m"(tss->edx), "m"(tss->esi), "m"(tss->edi)
        : "memory");
}

void switch_to(int n) {
    assert(n >= 0 && n < NR_TASKS && task_list[n] != NULL);
    if (current == task_list[n])
        return;

    // save_state(&current->tss);
    current = task_list[n];
    // 还没做TSS的初始化，也就是fork，后面做完fork的系统调用再打开注释
    // load_state(&current->tss);

    // 跳转到新任务的 EIP
    asm volatile("jmp *%0\n" : : "m"(current->tss.eip));
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
    panic("NO READY TASK??\n");
}

void task1() { //73402
    start_interrupt();
    while (true) {
        printk("A");
        yield();
    }
}

void task2() { //73453
    start_interrupt();
    while (true) {
        printk("B");
        yield();
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
    pid = task_create(task1, "testA", 5, KERNEL_USER);
    pid = task_create(task2, "testB", 5, KERNEL_USER);
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
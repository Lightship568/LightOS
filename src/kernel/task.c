#include <lib/debug.h>
#include <lib/list.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/syscall.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>

task_t* task_list[NR_TASKS] = {0};
task_t* current = (task_t*)NULL;
extern u32 volatile jiffies;  // clock.c 时间片数
extern u32 jiffy;             // clock.c 时钟中断的ms间隔

static list_t sleep_list;  // 睡眠任务链表

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

extern void save_state(struct tss_t* tss, void* eip);
extern void load_state(struct tss_t* tss);

void switch_to(int n) {
    assert(n >= 0 && n < NR_TASKS && task_list[n] != NULL);
    if (current == task_list[n])
        return;
    save_state(&current->tss, &&resume_execution);
    current = task_list[n];
    load_state(&current->tss);
resume_execution:
    // 从这里恢复执行
}

void schedule() {
    // 获取 current 的 pid
    pid_t n = current->pid + 1;
    for (int i = 0; i < NR_TASKS; ++i, ++n) {
        n %= NR_TASKS;
        if (task_list[n] != NULL && task_list[n]->ticks &&
            task_list[n]->state == TASK_READY) {
            switch_to(n);
            return;
        }
    }
    switch_to(0);
}
#define INTERVEL 10000000

void task1() {  // 73402
    start_interrupt();
    while (true) {
        for (int j = 0; j < 5; ++j) {
            printk("A");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

void task2() {  // 73453
    start_interrupt();
    while (true) {
        for (int j = 0; j < 5; ++j) {
            printk("B");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

void task_sleep_test() {
    int i = 0;
    while (true) {
        printk("taskB try to sleep 1000 ms!, times %d\n", ++i);
        sleep(1000);
    }
}

void idle() {
    while (true) {
        start_interrupt();
        asm volatile("hlt\n");
        schedule();
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

void task_init(void) {
    list_init(&sleep_list);
}

void task_test(void) {
    pid_t pid;
    pid = task_create(idle, "idle", 1, KERNEL_USER);
    pid = task_create(task_sleep_test, "testB", 5, KERNEL_USER);

    current = task_list[0];
    // 写入进程0栈信息
    asm volatile(
        "mov %0, %%esp\n"                               // 恢复 esp
        "mov %1, %%ebp\n"                               // 恢复 ebp
        :                                               // 输出约束为空
        : "m"(current->tss.esp), "m"(current->tss.ebp)  // 输入约束
        : "memory"  // 告诉编译器汇编代码会修改内存
    );
    idle();
}

void sys_yield(void) {
    schedule();
}

void sys_block(task_t* task, list_t* blist, task_state_t state) {}
void sys_unblock(task_t* task) {}

void sys_sleep(u32 ms) {
    assert(!get_interrupt_state());  // 确保是系统调用进来中断关闭的状态
    list_node_t* current_p = &current->node;
    task_t* target_task;
    list_node_t* anchor = &sleep_list.tail;

    // 没有被阻塞，才可被休眠
    assert(current->node.next == NULL && current->node.prev == NULL);

    current->ticks = ms / jiffy;
    current->jiffies = current->jiffies + current->ticks;

    // 基于ticks的插入排序
    for (list_node_t* p = sleep_list.head.next; p != &sleep_list.tail;
         p = p->next) {
        target_task = element_entry(task_t, node, p);
        if (current->jiffies < target_task->jiffies) {
            anchor = p;
            break;
        }
    }
    list_insert_before(anchor, current_p);

    current->state = TASK_SLEEPING;
    schedule();
}

// 非系统调用，但与sleep对应
void task_wakeup(void) {
    task_t* target_task;
    list_node_t* p = sleep_list.head.next;
    list_node_t* tmp_p;
    for (; p != &sleep_list.tail;) {
        task_t* target_task = element_entry(task_t, node, p);
        // 插入排序的，找目标jiffies <= 当前实际值的，证明要唤醒
        if (target_task->jiffies > jiffies) {
            break;
        }
        target_task->ticks = target_task->priority;
        target_task->state = TASK_READY;
        tmp_p = p->next;
        list_remove(p);
        p = tmp_p;
    }
}
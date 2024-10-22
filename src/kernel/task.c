#include <lib/debug.h>
#include <lib/list.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>
#include <lib/mutex.h>

extern u32 keyboard_read(char* buf, u32 count);
extern void init_kthread(void);

task_t* task_list[NR_TASKS] = {0};
task_t* current = (task_t*)NULL;
extern u32 volatile jiffies;  // clock.c 时间片数
extern u32 jiffy;             // clock.c 时钟中断的ms间隔

static list_t sleep_list;  // 睡眠任务链表
static list_t block_list;  // 阻塞任务链表
static mutex_t mutex_test; // 测试用的互斥量
static rwlock_t rwlock_test; // 测试用的读写锁

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

extern void switch_to(int n);

void schedule(void) {
    // 获取 current 的 pid
    pid_t n = current->pid + 1;
    pid_t target = 0;
    for (int i = 0; i < NR_TASKS; ++i, ++n) {
        n %= NR_TASKS;
        if (n != 0 && task_list[n] != NULL && task_list[n]->ticks &&
            task_list[n]->state == TASK_READY) {
                target = n;
                break;
        }
    }
    switch_to(target);
}
#define INTERVEL 10000000

void task1() {  // 73402
    while (true) {
        for (int j = 0; j < 5; ++j) {
            printk("A");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

void task2() {  // 73453
    while (true) {
        for (int j = 0; j < 5; ++j) {
            printk("B");
            for (int i = 0; i < INTERVEL; ++i);
        }
        // yield();
    }
}

void task_reader1() {
    int i = 0;
    while (true) {
        rwlock_read_lock(&rwlock_test);
        // printk("reader 1 get rwlock!, times %d\n", ++i);
        sys_sleep(2000);
        // printk("reader 1 wakeup, release rwlock\n");
        rwlock_read_unlock(&rwlock_test);
    }
}
void task_reader2() {
    int i = 0;
    while (true) {
        rwlock_read_lock(&rwlock_test);
        // printk("reader 2 get rwlock!, times %d\n", ++i);
        sys_sleep(2000);
        // printk("reader 2 wakeup, release rwlock\n");
        rwlock_read_unlock(&rwlock_test);
    }
}

void task_writer() {
    int i = 0;
    while (true) {
        rwlock_write_lock(&rwlock_test);
        // printk("writer get rwlock!, times %d\n", ++i);
        sys_sleep(3000);
        // printk("writer wakeup after 3000ms, release rwlock\n");
        rwlock_write_unlock(&rwlock_test);
    }
}
void idle(){
    // 写入进程0的栈信息（切栈）
    asm volatile(
        "mov %0, %%esp\n"                               // 恢复 esp
        "mov %1, %%ebp\n"                               // 恢复 ebp
        :                                               // 输出约束为空
        : "m"(current->tss.esp), "m"(current->tss.ebp)  // 输入约束
        : "memory"  // 告诉编译器汇编代码会修改内存
    );
    // 开中断，这样切进程就能保留当前中断状态方便恢复eflags
    start_interrupt();
    while (true) {
        schedule(); 
        asm volatile("hlt\n");
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
    task->brk = 0;
    task->magic = LIGHTOS_MAGIC;

    task->tss.eip = (u32)task_ptr;
    task->tss.ebp = stack;
    task->tss.esp = stack;

    // 硬件：用户态中断翻转特权级进入内核栈（ss0已经在全局tss中设置了）
    task->tss.esp0 = stack;

    task->state = TASK_READY;

    return pid;
}

void task_setup(void) {
    pid_t pid;
    pid = task_create(idle, "idle", 1, KERNEL_RING0);
    task_list[pid]->pde = KERNEL_PAGE_DIR_PADDR;
    pid = task_create(init_kthread, "init", 5, KERNEL_RING0);
    task_list[pid]->pde = KERNEL_PAGE_DIR_PADDR; // 暂时不应该切换页表，先让内核完成所有初始化
    // pid = task_create(task_reader1, "reader 1", 5, KERNEL_RING0);
    // pid = task_create(task_reader2, "reader 2", 5, KERNEL_RING0);
    // pid = task_create(task_writer, "writer", 5, KERNEL_RING0);

    current = task_list[0]; // IDLE
}

void task_init(void) {
    list_init(&sleep_list);
    list_init(&block_list);
    mutex_init(&mutex_test);
    rwlock_init(&rwlock_test);
    task_setup();
    DEBUGK("Task initialized\n");
}

void sys_yield(void) {
    schedule();
}

void sys_block(task_t* task, list_t* blist, task_state_t state) {}
void sys_unblock(task_t* task) {}

void sys_sleep(u32 ms) {
    assert(!get_interrupt_state());  // 确保是系统调用进来关中断的状态
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
    assert(!get_interrupt_state());  // 确保是clock进入的关中断状态
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

void task_block(task_t* task, list_t* waiting_list, task_state_t task_state){
    
    if (waiting_list == NULL){
        task->state = task_state;
        if (current == task){
            schedule();
            return;
        }
    }

    // 应该需要关中断，否则链表操作可能会崩，这里没法再使用互斥量了，否则catch-22
    bool intr = interrupt_disable();

    list_pushback(waiting_list, &task->node);
    task->state = task_state;
    if (current == task){
        schedule();
    }

    set_interrupt_state(intr);
}

void task_unblock(list_t* waiting_list){
    // 中断处理同上
    task_t* task;
    list_node_t* pnode;
    bool intr = interrupt_disable();
    
    if (!list_empty(waiting_list)){
        pnode = list_pop(waiting_list);
        task = element_entry(task_t, node, pnode);
        task->state = TASK_READY;
        schedule(); // 如果不主动让出，很可能循环获取资源导致阻塞的进程饿死
    }

    set_interrupt_state(intr);
}

void task_intr_unblock_no_waiting_list(task_t* task){
    // 中断中不可yield，不可主动让出执行流
    task->state = TASK_READY;
}
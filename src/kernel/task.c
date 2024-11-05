#include <lib/debug.h>
#include <lib/list.h>
#include <lib/mutex.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>
#include <lib/bitmap.h>
#include <lib/arena.h>

extern u32 keyboard_read(dev_t dev, char* buf, u32 count);
extern void init_kthread(void);

task_t* task_list[NR_TASKS] = {0};
task_t* current = (task_t*)NULL;
extern u32 volatile jiffies;  // clock.c 时间片数
extern u32 jiffy;             // clock.c 时钟中断的ms间隔

static list_t sleep_list;     // 睡眠任务链表
static list_t block_list;     // 阻塞任务链表
static mutex_t mutex_test;    // 测试用的互斥量
static rwlock_t rwlock_test;  // 测试用的读写锁

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
        if (n != 0 && task_list[n] && task_list[n]->ticks &&
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
            for (int i = 0; i < INTERVEL; ++i)
                ;
        }
        // yield();
    }
}

void task2() {  // 73453
    while (true) {
        for (int j = 0; j < 5; ++j) {
            printk("B");
            for (int i = 0; i < INTERVEL; ++i)
                ;
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
void idle(void) {
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
pid_t task_create(void (*eip_ptr)(void),
                  const char* name,
                  u32 priority,
                  u32 uid) {
    pid_t pid = get_free_task();
    task_t* task = task_list[pid];
    memset(task, 0, PAGE_SIZE);

    task->pid = pid;

    // todo wstrcpy name
    assert(sizeof(name) <= 16);
    strcpy(task->name, name);

    u32 stack = (u32)task + PAGE_SIZE - 1;
    task->stack = (u32*)stack;
    task->jiffies = 0;
    task->ticks = priority;
    task->priority = priority;
    task->uid = uid;
    task->brk = 0;
    task->magic = LIGHTOS_MAGIC;

    task->tss.eip = (u32)eip_ptr;
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
    task_list[pid]->pde = KERNEL_PAGE_DIR_PADDR;  // 暂时不应该切换页表，先让内核完成所有初始化
    // pid = task_create(task_reader1, "reader 1", 5, KERNEL_RING0);
    // pid = task_create(task_reader2, "reader 2", 5, KERNEL_RING0);
    // pid = task_create(task_writer, "writer", 5, KERNEL_RING0);

    current = task_list[0];  // IDLE
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

u32 sys_getpid() {
    return get_current()->pid;
}

u32 sys_getppid() {
    return get_current()->ppid;
}

pid_t sys_waitpid(pid_t pid, int32* status, int32 options) {
    task_t* task = get_current();
    task_t* child = NULL;
    bool has_child;
    while(true){
        has_child = false;
        for(size_t i = 2; i < NR_TASKS; ++i){
            if (task_list[i] && task_list[i]->ppid == task->pid){
                has_child = true;
                child = task_list[i];
                break;
            }
        }
        if (has_child){
            if (child->state == TASK_DEAD){
                break;
            }else{
                task->waitpid = pid;
                task_block(task, NULL, TASK_WARTING);
                continue;
            }
        }
        // 未找到 child，直接返回-1
        DEBUGK("[waitpid] pid %n isn't chiled process of pid %n\n", pid, task->pid);
        return -1;
    }
    // 清空子进程 PCB，返回其 pid
    *status = child->status;
    task_list[child->pid] = NULL;
    free_kpage((u32)child, 1);
    return child->pid;
}

u32 sys_fork() {
    u32 pid;
    task_t* task = get_current();
    task_t* child;
    assert(task->uid == USER_RING3);

    pid = get_free_task();
    child = task_list[pid];
    
    // 拷贝 PCB + 内核栈
    memcpy(child, task, PAGE_SIZE);

    child->pid = pid;
    child->ppid = task->pid;
    child->ticks = child->priority; // 重置时间片
    child->state = TASK_READY; // fork 进来是中断关闭状态
    child->stack = (u32*)((u32)child + PAGE_SIZE -1);
    child->tss.eax = pid;
    child->tss.esp0 = (u32)child->stack;

    // 创建并拷贝 PD 和 PTs
    // copy_pde(child);
    copy_pte(child);

    // 拷贝vmap
    child->vmap = (bitmap_t*)kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    // 拷贝vmap buff
    child->vmap->bits = (void*)alloc_kpage(1);
    memcpy(child->vmap->bits, task->vmap->bits, PAGE_SIZE);

    // 不需要手动修改其他寄存器，中断上下文保存了寄存器状态，复制内核栈就可以自动恢复了

    child->tss.eip = (u32)&&child_task;

    task->tss.eax = pid;
    child->tss.eax = 0;

    //单独保存栈寄存器
    asm volatile (
        "mov %%esp, %0\n"        // 保存 esp
        "mov %%ebp, %1\n"        // 保存 ebp
        : "=m"(child->tss.esp), "=m"(child->tss.ebp)  // 输出约束
        :                        // 无输入寄存器
        : "memory"               // 汇编可能修改内存
    );
    // 迁移内核栈
    child->tss.esp = (0xfff & child->tss.esp) | (0xfffff000 & (u32)child->stack);
    child->tss.ebp = (0xfff & child->tss.ebp) | (0xfffff000 & (u32)child->stack);


child_task:
    // 返回值
    task = get_current();
    // asm volatile("mov %0, %%eax\n"::"r"(task->tss.eax):"memory");
    return task->tss.eax;
}

u32 sys_exit(u32 status){
    // 需要释放的有：vmap, vmap->buf, PD, PTs, 以及 PTs 指向的所有 user_page
    // 无需释放：task_list[n], task_t与内核栈所在页。这是因为延迟回收策略，便于父进程 waitpid
    task_t* task = get_current();
    // 主动调用exit()的情况，应该确认程序没有阻塞
    assert(task->state == TASK_RUNNING);
    assert(task->pid != 0 && task->pid != 1);
    
    task->state = TASK_DEAD;
    task->status = status;

    // 释放所有user_page, PTs, PD
    free_pte(task);

    // 释放vmap->buf
    free_kpage((u32)task->vmap->bits, 1);
    // 释放vmap
    kfree(task->vmap);

    for (size_t i = 0; i < NR_TASKS; ++i){
        if(!task_list[i]) continue;
        
        if (task_list[i]->ppid == task->pid){
            // 将子进程托管给爷进程
            task_list[i]->ppid = task->ppid;
        } else if (task_list[i]->waitpid == task->pid){
            // 唤醒 TASK_WAIT 的父进程
            assert(task_list[i]->state == TASK_WARTING);
            task_intr_unblock_no_waiting_list(task_list[i]);
            task_list[i]->waitpid = 0;
        }
        // todo 若父进程是init，则清理僵尸进程
        
    }

    DEBUGK("Process %d (0x%x) exit with status %d\n", task->pid, task, task->status);
    schedule();
}

void sys_sleep(u32 ms) {
    assert(!get_interrupt_state());  // 确保是系统调用进来关中断的状态
    list_node_t* current_p = &current->node;
    task_t* target_task;
    list_node_t* anchor = &sleep_list.tail;

    // 没有被阻塞，才可被休眠
    assert(current->node.next == NULL && current->node.prev == NULL);

    current->ticks = ms / jiffy;
    current->jiffies = current->jiffies + current->ticks;

    // 基于 jiffies 的插入排序
    list_insert_sort(&sleep_list, current_p, element_node_offset(task_t, node, jiffies));

    current->state = TASK_SLEEPING;
    schedule();
}

// 非系统调用，但与sleep对应，被 clock.c 时钟中断调用
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

void task_block(task_t* task, list_t* waiting_list, task_state_t task_state) {
    if (waiting_list == NULL) {
        task->state = task_state;
        if (current == task) {
            schedule();
            return;
        }
    }

    // 应该需要关中断，否则链表操作可能会崩，这里没法再使用互斥量了，否则catch-22
    bool intr = interrupt_disable();

    list_pushback(waiting_list, &task->node);
    task->state = task_state;
    if (current == task) {
        schedule();
    }

    set_interrupt_state(intr);
}

void task_unblock(list_t* waiting_list) {
    // 中断处理同上
    task_t* task;
    list_node_t* pnode;
    bool intr = interrupt_disable();

    if (!list_empty(waiting_list)) {
        pnode = list_pop(waiting_list);
        task = element_entry(task_t, node, pnode);
        assert(task->magic == LIGHTOS_MAGIC);
        task->state = TASK_READY;
        schedule();  // 如果不主动让出，很可能循环获取资源导致阻塞的进程饿死
    }

    set_interrupt_state(intr);
}

void task_intr_unblock_no_waiting_list(task_t* task) {
    // 中断中不可yield，不可主动让出执行流
    if (task->node.next != NULL){
        assert(task->node.prev != NULL);
        list_remove(&task->node);
    }
    task->state = TASK_READY;
}
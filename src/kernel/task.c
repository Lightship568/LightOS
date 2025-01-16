#include <lib/arena.h>
#include <lib/bitmap.h>
#include <lib/debug.h>
#include <lib/list.h>
#include <lib/mutex.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/fs.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <lightos/timer.h>
#include <lightos/tty.h>
#include <sys/assert.h>
#include <sys/errno.h>
#include <sys/global.h>
#include <sys/types.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern void init_kthread(void);

task_t* task_list[TASK_NR] = {0};
task_t* current = (task_t*)NULL;
extern u32 volatile jiffies;  // clock.c 时间片数
extern u32 jiffy;             // clock.c 时钟中断的ms间隔

static list_t block_list;     // 阻塞任务链表
static mutex_t mutex_test;    // 测试用的互斥量
static rwlock_t rwlock_test;  // 测试用的读写锁

task_t* get_task(pid_t pid) {
    for (size_t i = 0; i < TASK_NR; i++) {
        if (!task_list[i]) {
            continue;
        }
        if (task_list[i]->pid == pid) {
            return task_list[i];
        }
    }
    return NULL;
}

task_t* get_current() {
    return current;
}

// 分配一个内核页用于存放 pcb 与 内核栈，返回 pid，若达到 TASK_NR 则返回 -1
static pid_t get_free_task() {
    for (int i = 0; i < TASK_NR; ++i) {
        if (task_list[i] == NULL) {
            task_list[i] = (task_t*)alloc_kpage(1);
            return i;
        }
    }
    panic("Task count reach top limit of %d, can't create a new task!\n",
          TASK_NR);
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
    for (int i = 0; i < TASK_NR; ++i, ++n) {
        n %= TASK_NR;
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
    LOGK("Run IDLE & Start interrupt\n");
    // 开中断，这样切进程就能保留当前中断状态方便恢复eflags
    start_interrupt();
    while (true) {
        schedule();
        asm volatile("hlt\n");
    }
}

extern file_t file_table[];

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

    u32 stack = (u32)task + PAGE_SIZE;
    task->stack = (u32*)stack;
    task->jiffies = 0;
    task->ticks = priority;
    task->priority = priority;
    task->uid = uid;
    task->gid = 0;  // todo: group
    task->brk = USER_EXEC_ADDR;
    task->text = USER_EXEC_ADDR;
    task->data = USER_EXEC_ADDR;
    task->end = USER_EXEC_ADDR;
    task->iexec = NULL;
    task->iroot = task->ipwd = get_root_inode();
    task->umask = 0022;  // 0755
    task->iroot->count += 2;

    // 当前工作目录
    task->pwd_len = 1;
    task->pwd = (char*)kmalloc(task->pwd_len);
    task->pwd[0] = '/';

    // 标准流
    task->files[STDIN_FILENO] = &file_table[STDIN_FILENO];
    task->files[STDOUT_FILENO] = &file_table[STDOUT_FILENO];
    task->files[STDERR_FILENO] = &file_table[STDERR_FILENO];
    task->files[STDIN_FILENO]->count++;
    task->files[STDOUT_FILENO]->count++;
    task->files[STDERR_FILENO]->count++;

    // 魔数
    task->magic = LIGHTOS_MAGIC;

    // 初始化信号
    task->signal = 0;
    task->blocked = 0;
    for (size_t i = 0; i < MAXSIG; i++) {
        sigaction_t* action = &task->actions[i];
        action->flags = 0;
        action->mask = 0;
        action->handler = SIG_DFL;
        action->restorer = NULL;
    }

    // 关键 tss 初始化
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
    task_list[pid]->pde =
        KERNEL_PAGE_DIR_PADDR;  // 暂时不应该切换页表，先让内核完成所有初始化
    // pid = task_create(task_reader1, "reader 1", 5, KERNEL_RING0);
    // pid = task_create(task_reader2, "reader 2", 5, KERNEL_RING0);
    // pid = task_create(task_writer, "writer", 5, KERNEL_RING0);

    current = task_list[0];  // IDLE
}

void task_init(void) {
    list_init(&block_list);
    mutex_init(&mutex_test);
    rwlock_init(&rwlock_test);
    task_setup();
    LOGK("Task initialized\n");
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
    while (true) {
        has_child = false;
        for (size_t i = 2; i < TASK_NR; ++i) {
            if (task_list[i] && task_list[i]->ppid == task->pid) {
                has_child = true;
                child = task_list[i];
                break;
            }
        }
        if (has_child) {
            if (child->state == TASK_DEAD) {
                break;
            } else {
                task->waitpid = pid;
                task_block(task, NULL, TASK_WAITING);
                continue;
            }
        }
        // 未找到 child，直接返回-1
        LOGK("[waitpid] pid %n isn't chiled process of pid %n\n", pid,
             task->pid);
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
    child->ticks = child->priority;  // 重置时间片
    child->state = TASK_READY;       // fork 进来是中断关闭状态
    child->stack = (u32*)((u32)child + PAGE_SIZE);
    child->tss.eax = pid;
    child->tss.esp0 = (u32)child->stack;

    // 创建并拷贝 PTs（拷贝 PCB 使 pde 共享)
    copy_pte(child);

    // 拷贝vmap
    child->vmap = (bitmap_t*)kmalloc(sizeof(bitmap_t));
    memcpy(child->vmap, task->vmap, sizeof(bitmap_t));

    // 拷贝vmap buff
    child->vmap->bits = (void*)alloc_kpage(1);
    memcpy(child->vmap->bits, task->vmap->bits, PAGE_SIZE);

    // 拷贝 pwd
    child->pwd = kmalloc(task->pwd_len);
    child->pwd_len = task->pwd_len;
    memcpy(child->pwd, task->pwd, task->pwd_len);

    // 工作目录引用计数
    task->ipwd->count++;
    task->iroot->count++;
    // 本体二进制引用计数
    if (task->iexec) {
        task->iexec->count++;
    }

    // 文件引用计数
    for (size_t i = 0; i < TASK_FILE_NR; ++i) {
        file_t* file = child->files[i];
        if (file) {
            file->count++;
        }
    }

    // 不需要手动修改其他寄存器，中断上下文保存了寄存器状态，复制内核栈就可以自动恢复了

    child->tss.eip = (u32) && child_task;

    task->tss.eax = pid;
    child->tss.eax = 0;

    // 单独保存栈寄存器
    asm volatile(
        "mov %%esp, %0\n"                             // 保存 esp
        "mov %%ebp, %1\n"                             // 保存 ebp
        : "=m"(child->tss.esp), "=m"(child->tss.ebp)  // 输出约束
        :                                             // 无输入寄存器
        : "memory"  // 汇编可能修改内存
    );
    // 迁移内核栈
    child->tss.esp =
        (0xfff & child->tss.esp) | (u32)child;
    child->tss.ebp =
        (0xfff & child->tss.ebp) | (u32)child;

child_task:
    // 返回值
    task = get_current();
    // asm volatile("mov %0, %%eax\n"::"r"(task->tss.eax):"memory");
    return task->tss.eax;
}

// 子进程退出，信号通知父进程
static void task_tell_father(task_t* task) {
    if (!task->ppid) {
        return;
    }
    for (size_t i = 0; i < TASK_NR; i++) {
        task_t* parent = task_list[i];
        if (!parent) {
            continue;
        }
        if (parent->pid != task->ppid) {
            continue;
        }
        parent->signal |= SIGMASK(SIGCHLD);
        return;
    }
    panic("task %d cannot find parent %d\n", task->pid, task->ppid);
}

u32 sys_exit(u32 status) {
    // 需要释放的有：vmap, vmap->buf, PD, PTs, 以及 PTs 指向的所有
    // user_page，以及 mmap 无需释放：task_list[n],
    // task_t与内核栈所在页。这是因为延迟回收策略，便于父进程 waitpid
    task_t* task = get_current();
    // 主动调用exit()的情况，应该确认程序没有阻塞
    assert(task->state == TASK_RUNNING);
    assert(task->pid != 0 && task->pid != 1);

    task->state = TASK_DEAD;
    task->status = status;

    // 释放所有user_page, PTs, PD
    free_pte(task);
    free_kpage(task->pde + KERNEL_VADDR_OFFSET, 1);

    // todo 释放掉 vmap 指向的所有 mmap 空间和文件

    // 释放vmap->buf
    free_kpage((u32)task->vmap->bits, 1);
    // 释放vmap
    kfree(task->vmap);
    // 释放 pwd
    kfree(task->pwd);

    // 引用计数释放
    iput(task->ipwd);
    iput(task->iroot);

    // 释放本体二进制inode
    iput(task->iexec);

    // 释放所有 timer
    timer_remove(task);

    // 信号通知父进程
    task_tell_father(task);

    if (task_sess_leader(task)) {
        // 向会话中所有进程发送 SIGHUP
        for (pid_t i = 0; i < TASK_NR; ++i) {
            task_t* tmptask = task_list[i];
            if (!tmptask || tmptask->sid != task->sid ||
                tmptask->pid == task->pid) {
                continue;
            }
            tmptask->signal |= SIGMASK(SIGHUP);
        }
        // 清空与该 session 绑定的 tty 设备的前台进程组
        if (task->tty > 0) {
            device_t* device = device_get(task->tty);
            tty_t* tty = (tty_t*)device->ptr;
            tty->pgid = 0;
        }
    }

    // 释放所有打开文件
    for (size_t i = 0; i < TASK_FILE_NR; ++i) {
        file_t* file = task->files[i];
        if (file) {  // 标准流也要释放引用计数
            sys_close(i);
        }
    }

    for (size_t i = 0; i < TASK_NR; ++i) {
        if (!task_list[i])
            continue;

        if (task_list[i]->ppid == task->pid) {
            // 将子进程托管给爷进程
            task_list[i]->ppid = task->ppid;
        } else if (task_list[i]->waitpid == task->pid) {
            // 唤醒 TASK_WAIT 的父进程，当然也可能不是 waiting 状态
            if (task_list[i]->state == TASK_WAITING) {
                task_intr_unblock_no_waiting_list(task_list[i]);
            }
            task_list[i]->waitpid = 0;
        }
        // todo 若父进程是init，则清理僵尸进程
    }

    LOGK("Process %s (pid %d) exit with status %d\n", task->name, task->pid,
         task->status);
    schedule();
}

// 超时后被 timer 以 handler 形式调用
void task_wakeup(timer_t* timer) {
    assert(!get_interrupt_state());  // 确保是clock进入的关中断状态
    task_t* task = timer->task;
    task->ticks = task->priority;
    task->state = TASK_READY;
}

void sys_sleep(u32 ms) {
    assert(!get_interrupt_state());  // 确保是系统调用进来关中断的状态
    // 没有被阻塞，才可被休眠
    assert(current->node.next == NULL && current->node.prev == NULL);

    timer_add(ms, task_wakeup, NULL);

    current->state = TASK_SLEEPING;
    schedule();
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
    if (task->node.next != NULL) {
        assert(task->node.prev != NULL);
        list_remove(&task->node);
    }
    task->state = TASK_READY;
}

fd_t task_get_fd(task_t* task) {
    fd_t i;
    for (i = 3; i < TASK_FILE_NR; ++i) {
        if (!task->files[i]) {
            break;
        }
    }
    if (i == TASK_FILE_NR) {
        panic("Task pid %d(%s) open files out of TASK_FILE_NR %d\n", task->pid,
              task->name, TASK_FILE_NR);
    }
    return i;
}

void task_put_fd(task_t* task, fd_t fd) {
    assert(fd < TASK_FILE_NR);
    task->files[fd] = NULL;
}

bool task_sess_leader(task_t* task) {
    return task->sid == task->pid;
}

pid_t sys_setsid(void) {
    task_t* task = get_current();
    // 会话首领不可开启新会话
    if (task_sess_leader(task)) {
        return -EPERM;
    }
    task->sid = task->pid;
    task->pgid = task->pid;

    return task->sid;
}

int sys_setpgid(pid_t pid, pid_t pgid) {
    task_t* current_task = get_current();
    task_t* task = NULL;
    if (!pid) {
        // pid == 0，修改当前程序
        task = current_task;
    } else {
        // 找到目标 pid 的 pcb
        for (int i = 0; i < TASK_NR; i++) {
            if (!task_list[i] || task_list[i]->pid != pid) {
                continue;
            }
            task = task_list[i];
            break;
        }
    }
    // pgid == 0，修改为当前程序组
    if (!pgid) {
        pgid = current_task->pid;
    }
    if (!task) {
        return -ESRCH;
    }
    // 会话首领不可修改进程组
    if (task_sess_leader(task)) {
        return -EPERM;
    }
    // 不可修改其他会话的进程
    if (task->sid != current_task->sid) {
        return -EPERM;
    }
    task->pgid = pgid;
    return EOK;
}

pid_t sys_getpgrp(void) {
    return get_current()->pgid;
}
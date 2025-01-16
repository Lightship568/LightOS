#include <lightos/task.h>
#include <sys/types.h>

#include <lib/debug.h>
#include <lib/signal.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <sys/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

typedef struct signal_frame_t {
    u32 restorer;  // 恢复函数
    u32 sig;       // 信号
    u32 blocked;   // 屏蔽位图
    // 保存调用时的寄存器，用于恢复执行信号之前的代码
    u32 eax;
    u32 ecx;
    u32 edx;
    u32 eflags;
    u32 eip;
} signal_frame_t;

// 获取信号屏蔽位图
int sys_sgetmask(void) {
    task_t* task = get_current();
    return task->blocked;
}

// 设置信号屏蔽位图
int sys_ssetmask(int newmask) {
    if (newmask == EOF) {
        return -EPERM;
    }
    task_t* task = get_current();
    int old = task->blocked;
    task->blocked = newmask & ~SIGMASK(SIGKILL);
    return old;
}

// 注册信号处理函数
int sys_signal(int sig, int handler, int restorer) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;
    task_t* task = get_current();
    sigaction_t* ptr = &task->actions[sig - 1];
    ptr->mask = 0;
    ptr->handler = (void (*)(int))handler;
    ptr->flags = SIG_ONESHOT | SIG_NOMASK;
    ptr->restorer = (void (*)())restorer;
    return handler;
}

// 注册信号处理函数，更高级的一种方式
int sys_sigaction(int sig, sigaction_t* action, sigaction_t* oldaction) {
    if (sig < MINSIG || sig > MAXSIG || sig == SIGKILL)
        return EOF;
    task_t* task = get_current();
    sigaction_t* ptr = &task->actions[sig - 1];
    if (oldaction)
        *oldaction = *ptr;

    *ptr = *action;
    if (ptr->flags & SIG_NOMASK)
        ptr->mask = 0;
    else
        ptr->mask |= SIGMASK(sig);
    return 0;
}

// 发送信号，程序可以捕获，不一定会退出
int sys_kill(pid_t pid, int sig) {
    if (sig < MINSIG || sig > MAXSIG) {
        return EOF;
    }
    task_t* task = get_task(pid);
    if (!task || task->pid == 1 || task->uid == KERNEL_RING0) {
        return EOF;
    }

    LOGK("Send signal %d to task %s pid %d \n", sig, task->name, pid);
    task->signal |= SIGMASK(sig);
    if (task->state == TASK_WAITING || task->state == TASK_SLEEPING) {
        task_intr_unblock_no_waiting_list(task);
    }
    return 0;
}

// 内核信号处理函数，在 interrupt_exit 过程被调用
void task_signal(void) {
    task_t* task = get_current();
    // 获得任务可用信号位图
    u32 map = task->signal & (~task->blocked);
    if (!map) {
        return;
    }
    assert(task->uid);
    int sig = 1;
    for (; sig <= MAXSIG; sig++) {
        if (map & SIGMASK(sig)) {
            // 将此信号置空，表示已执行
            task->signal &= (~SIGMASK(sig));
            break;
        }
    }
    LOGK("Handle task %d signal %x in interrupt_exit\n", task->pid, sig);

    // 得到对应的信号处理结构
    sigaction_t* action = &task->actions[sig - 1];
    // 忽略信号
    if (action->handler == SIG_IGN) {
        return;
    }
    // 子进程终止的默认处理方式
    if (action->handler == SIG_DFL && sig == SIGCHLD) {
        return;
    }
    // 默认信号处理方式，为退出
    if (action->handler == SIG_DFL) {
        sys_exit(SIGMASK(sig));
    }

    // 处理用户态栈，使得程序去执行信号处理函数，处理结束之后，调用 restorer
    // 为什么获取到的 iframe 内的参数都不对？
    intr_frame_t* iframe =
        (intr_frame_t*)((u32)task + PAGE_SIZE - sizeof(intr_frame_t));

    // 这个 sig_frame 是存放在用户栈顶的
    signal_frame_t* sig_frame =
        (signal_frame_t*)(iframe->esp - sizeof(signal_frame_t));

    // 保存执行前的寄存器
    sig_frame->eip = iframe->eip;
    sig_frame->eflags = iframe->eflags;
    sig_frame->edx = iframe->edx;
    sig_frame->ecx = iframe->ecx;
    sig_frame->eax = iframe->eax;

    // 屏蔽所有信号
    sig_frame->blocked = EOF;

    // 不屏蔽在信号处理过程中，再次收到该信号
    if (action->flags & SIG_NOMASK) {
        sig_frame->blocked = task->blocked;
    }

    // 信号
    sig_frame->sig = sig;
    // 信号处理结束的恢复函数
    sig_frame->restorer = (u32)action->restorer;

    // 设置用户态的 esp 指向 sig_frame 便于定位并恢复上下文
    iframe->esp = (u32)sig_frame;
    // eip 设置为 handler
    iframe->eip = (u32)action->handler;

    // 如果设置了 ONESHOT，表示该信号只执行一次
    if (action->flags & SIG_ONESHOT) {
        action->handler = SIG_DFL;
    }

    // 进程屏蔽码添加此信号
    task->blocked |= action->mask;
}

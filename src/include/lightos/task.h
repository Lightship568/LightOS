#ifndef LIGHTOS_TASK_H
#define LIGHTOS_TASK_H

/**
 * 进程管理
 */

#include <lib/list.h>
#include <sys/global.h>
#include <sys/types.h>

#define NR_TASKS 64

#define KERNEL_RING0 0
#define USER_RING3 1000

// 进程名最大长度
#define TASK_NAME_LEN 16
// 进程打开文件的最大数量
#define TASK_FILE_NR 16

// onix 最终版 task_struct
// typedef struct task_t
// {
//     u32 *stack;                         // 内核栈
//     list_node_t node;                   // 任务阻塞节点
//     task_state_t state;                 // 任务状态
//     u32 priority;                       // 任务优先级
//     int ticks;                          // 剩余时间片
//     u32 jiffies;                        // 上次执行时全局时间片
//     char name[TASK_NAME_LEN];           // 任务名
//     u32 uid;                            // 用户 id
//     u32 gid;                            // 用户组 id
//     pid_t pid;                          // 任务 id
//     pid_t ppid;                         // 父任务 id
//     pid_t pgid;                         // 进程组
//     pid_t sid;                          // 进程会话
//     dev_t tty;                          // tty 设备
//     u32 pde;                            // 页目录物理地址
//     struct bitmap_t *vmap;              // 进程虚拟内存位图
//     u32 text;                           // 代码段地址
//     u32 data;                           // 数据段地址
//     u32 end;                            // 程序结束地址
//     u32 brk;                            // 进程堆内存最高地址
//     int status;                         // 进程特殊状态
//     pid_t waitpid;                      // 进程等待的 pid
//     char *pwd;                          // 进程当前目录
//     struct inode_t *ipwd;               // 进程当前目录 inode program work
//     directory struct inode_t *iroot;              // 进程根目录 inode struct
//     inode_t *iexec;              // 程序文件 inode u16 umask; // 进程用户权限
//     struct file_t *files[TASK_FILE_NR]; // 进程文件表
//     u32 signal;                         // 进程信号位图
//     u32 blocked;                        // 进程信号屏蔽位图
//     struct timer_t *alarm;              // 闹钟定时器
//     struct timer_t *timer;              // 超时定时器
//     sigaction_t actions[MAXSIG];        // 信号处理函数
//     struct fpu_t *fpu;                  // fpu 指针
//     u32 flags;                          // 特殊标记
//     u32 magic;                          // 内核魔数，用于检测栈溢出
// } task_t;

typedef enum task_state_t {
    TASK_INIT,      // 初始化
    TASK_RUNNING,   // 执行
    TASK_READY,     // 就绪
    TASK_BLOCKED,   // 阻塞
    TASK_SLEEPING,  // 阻塞-睡眠
    TASK_WARTING,   // 阻塞-等待
    TASK_DEAD,      // 死亡
} task_state_t;

// PCB
typedef struct task_t {
    u32* stack;                          // 内核栈
    task_state_t state;                  // 任务状态
    u32 priority;                        // 任务优先级
    u32 ticks;                           // 剩余时间片
    u32 jiffies;                         // 上次执行时全局时间片
    pid_t pid;                           // 任务ID
    pid_t ppid;                          // 父任务ID
    char name[TASK_NAME_LEN];            // 任务名
    u32 uid;                             // 用户ID
    u32 gid;                             // 组ID
    u32 pde;                             // 页目录物理地址
    list_node_t node;                    // 链表
    struct bitmap_t* vmap;               // 进程虚拟内存位图
    tss_t tss;                           // TSS
    u32 brk;                             // 进程堆内存最高地址
    int32 status;                        // 进程特殊状态
    pid_t waitpid;                       // 进程等待的 pid
    struct inode_t* ipwd;                // 进程当前目录 inode
    struct inode_t* iroot;               // 进程根目录 inode
    u16 umask;                           // 进程用户权限
    struct file_t* files[TASK_FILE_NR];  // 进程文件表
    u32 magic;  // 检测内核栈溢出（溢出到 PCB 就寄了）
} task_t;

// 中断帧，用于move_to_user 的 ROP
typedef struct intr_frame_t {
    u32 vector;
    u32 edi;
    u32 esi;
    u32 ebp;
    u32 esp_dummy;  // 虽然 pushad 把 esp 也压入，但 esp 是不断变化的，所以会被
                    // popad 忽略
    u32 ebx;
    u32 edx;
    u32 ecx;
    u32 eax;
    u32 gs;
    u32 fs;
    u32 es;
    u32 ds;
    u32 vector0;
    u32 error;  // error / magic
    u32 eip;
    u32 cs;
    u32 eflags;
    u32 esp;
    u32 ss;
} intr_frame_t;

extern task_t* task_list[NR_TASKS];
extern task_t* current;

void task_init(void);

// 返回 current
task_t* get_current();

// 调度函数
void schedule(void);

// 切换上下文
void switch_to(int n);

// 系统调用 sys_yield
void sys_yield(void);

// 系统调用 sys_sleep
void sys_sleep(u32 ms);

// 系统调用
u32 sys_getpid();
u32 sys_getppid();

// 系统调用 fork
u32 sys_fork();
// 系统调用 exit
u32 sys_exit(u32 status);

pid_t sys_waitpid(pid_t pid, int32* status, int32 options);

// 非系统调用，但与sleep对应，被clock调用
void task_wakeup(void);

// 非系统调用，阻塞任务并加入对应阻塞链表
// waiting_list 可以为 NULL，此时不关中断，只设置 task 本身并调度
void task_block(task_t* task, list_t* waiting_list, task_state_t task_state);
// 任务恢复需要传入阻塞链表，默认FIFO
void task_unblock(list_t* waiting_list);
// 【中断中使用】如果没有阻塞链表，可以单独传递task指针，且没有yield，不会让出执行流
void task_intr_unblock_no_waiting_list(task_t* task);

#endif
#ifndef LIGHTOS_TASK_H
#define LIGHTOS_TASK_H

/**
 * 进程管理
*/

#include <sys/types.h>

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
//     struct inode_t *ipwd;               // 进程当前目录 inode program work directory
//     struct inode_t *iroot;              // 进程根目录 inode
//     struct inode_t *iexec;              // 程序文件 inode
//     u16 umask;                          // 进程用户权限
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


typedef struct tss_struct
{
    void (*eip)(void);
    u32 edi;
    u32 esi;
    u32 ebx;
    u32 ebp;
} tss_struct;

typedef struct task_t{
    u32 *stack;                         // 内核栈
    u32 *base;
    tss_struct tss;
} task_t;


void task_test_init(void);
void task_schedule(void);

#endif
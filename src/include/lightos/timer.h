#ifndef LIGHTOS_TIMER_H
#define LIGHTOS_TIMER_H

#include <sys/types.h>
#include <lib/list.h>

typedef void *handler_t;

typedef struct timer_t
{
    list_node_t node;                  // 链表节点
    struct task_t *task;               // 相关任务
    u32 expires;                       // 超时时间
    void (*handler)(struct timer_t *); // 超时处理函数
    void *arg;                         // 参数
    bool active;                       // 激活状态
} timer_t;

// 添加定时器
timer_t *timer_add(u32 expire_ms, handler_t handler, void *arg);
// 释放定时器
void timer_put(timer_t *timer);
// 唤醒定时器
void timer_wakeup(void);
// 移除 task 相关的全部定时器
void timer_remove(struct task_t *task);
// 更新定时器超时
void timer_update(timer_t *timer, u32 expire_ms);
// 获取超时时间片
int timer_expire_jiffies(u32 expire_ms);
// 判断是否超时
bool timer_is_expires(u32 expires);

void timer_init(void);

#endif
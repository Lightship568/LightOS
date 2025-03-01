#include <lib/debug.h>
#include <lightos/timer.h>
#include <sys/errno.h>
#include <lightos/task.h>
#include <lib/arena.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

extern u32 volatile jiffies;
extern u32 jiffy;

static list_t timer_list;

static timer_t* timer_get() {
    timer_t* timer = (timer_t*)kmalloc(sizeof(timer_t));
    return timer;
}

void timer_put(timer_t* timer) {
    list_remove(&timer->node);
    kfree(timer);
}

// 如果没有传入 handler，就默认解除阻塞，没有其他操作
void default_timeout(timer_t* timer) {
    assert(timer->task->node.next);
    // task_intr_unblock_no_waiting_list(timer->task, -ETIME);
    task_intr_unblock_no_waiting_list(timer->task);
}

timer_t* timer_add(u32 expire_ms, handler_t handler, void* arg) {
    timer_t* timer = timer_get();
    timer->task = get_current();
    timer->expires = timer_expire_jiffies(expire_ms);
    timer->handler = handler;
    timer->arg = arg;
    timer->active = false;

    list_insert_sort(&timer_list, &timer->node,
                     element_node_offset(timer_t, node, expires));
    return timer;
}

// 更新定时器超时
void timer_update(timer_t* timer, u32 expire_ms) {
    list_remove(&timer->node);
    timer->expires = jiffies + expire_ms / jiffy;
    list_insert_sort(&timer_list, &timer->node,
                     element_node_offset(timer_t, node, expires));
}

u32 timer_expires() {
    if (list_empty(&timer_list)) {
        return EOF;
    }
    timer_t* timer = element_entry(timer_t, node, timer_list.head.next);
    return timer->expires;
}

// 得到超时时间片
int timer_expire_jiffies(u32 expire_ms) {
    return jiffies + expire_ms / jiffy;
}

// 判断是否已经超时
bool timer_is_expires(u32 expires) {
    return jiffies > expires;
}

// 从定时器链表中找到 task 任务的定时器，删除之，用于 task_exit
void timer_remove(task_t* task) {
    list_t* list = &timer_list;
    for (list_node_t* ptr = list->head.next; ptr != &list->tail;) {
        timer_t* timer = element_entry(timer_t, node, ptr);
        ptr = ptr->next;
        if (timer->task != task)
            continue;
        timer_put(timer);
    }
}

void timer_wakeup(void) {
    while (timer_expires() <= jiffies) {
        timer_t* timer = element_entry(timer_t, node, timer_list.head.next);
        timer->active = true;

        assert(timer->expires <= jiffies);
        if (timer->handler) {
            timer->handler(timer);
        } else {
            default_timeout(timer);
        }
        timer_put(timer);
    }
}

void timer_init(void) {
    list_init(&timer_list);
    LOGK("timer initialized\n");
}
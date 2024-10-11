/**
 * 互斥量
 * 一个 bool 实现原子操作
 * 一个 lint_t 保存待唤醒链表
 */
#include <lib/mutex.h>
#include <lightos/task.h>
#include <lib/print.h>

void mutex_init(mutex_t* mutex){
    mutex->value = false;
    list_init(&mutex->waiters);
}

void mutex_lock(mutex_t* mutex){
    // 个人认为这里不需要关中断，因为一旦互斥量释放，只会选择一个等待进程进行唤醒
    // 此时一定可以 inc 获得该互斥量，而多线程情况（新任务未阻塞且同时争抢互斥量）关中断也无效
    task_t* current = get_current();
    
    while(mutex->value){
        // printk("Task %s blocking...\n", current->name);
        task_block(current, &mutex->waiters, TASK_BLOCKED);
    }
    // printk("Task %s get mutex\n", current->name);
    mutex->value++; //inc 原子操作
}

void mutex_unlock(mutex_t* mutex){
    // 解锁也不需要关中断，一种情况：mutex--后瞬间被中断，新任务又恰好占用mutex，
    // 调度时阻塞任务被恢复会再次判断mutex是否被占用，并再次进入阻塞。
    task_t* current = get_current();
    
    // printk("Task %s release mutex\n", current->name);
    mutex->value--;
    task_unblock(&mutex->waiters);
}
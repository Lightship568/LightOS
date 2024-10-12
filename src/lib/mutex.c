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

void mutex_lock(mutex_t* mutex) {
    task_t* current = get_current();
    int expected = 0;
    int new_value = 1;

    // 若想不关中断，则必须让“检查+获取锁”是原子操作。
    // 否则若中断于二者中间，此时调度到其他程序就会出现同时有两个程序操作临界区的情况发生。
    while (true) {
        asm volatile(
            "lock cmpxchg %2, %1"
            : "=a"(expected), "+m"(mutex->value)
            : "r"(new_value), "0"(expected)
            : "memory");

        // 如果cmpxchg成功，则mutex->value已经从0变成了1
        if (expected == 0) {
            break;  // 成功获取锁，退出循环
        }

        // 如果未成功获取锁，任务进入阻塞
        task_block(current, &mutex->waiters, TASK_BLOCKED);
        expected = 0;  // 重置expected为0，用于下次比较
    }
}

void mutex_unlock(mutex_t* mutex){
    // 解锁也不需要关中断，一种情况：mutex--后瞬间被中断，新任务又恰好占用mutex，
    // 调度时阻塞任务被恢复会再次判断mutex是否被占用，并再次进入阻塞。
    task_t* current = get_current();
    
    // 直接减小mutex->value的值，解锁
    asm volatile(
        "lock dec %0"
        : "+m"(mutex->value)
        : 
        : "memory");

    task_unblock(&mutex->waiters);
}
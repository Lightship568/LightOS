/**
 * 互斥量
 * 一个 bool 实现原子操作
 * 一个 lint_t 保存待唤醒链表
 * 
 * 读写锁
 * readers + writer，自旋非阻塞实现。
 * writer 释放锁后主动 schedule 让出执行流
 */
#include <lib/mutex.h>
#include <lightos/task.h>
#include <lib/print.h>
#include <lib/syscall.h>

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

void rwlock_init(rwlock_t* lock){
    lock->writer = 0;
    lock->readers = 0;
}

/**
 * 读写锁的实现原理其实很简单，原子变量是writer，也就是写者状态。读者会尝试获取writer来测试是否有写者，之后立即释放，代表此时都是读者。
 * 写者则需要获取writer来阻塞所有后续读者，之后等待读者们逐步退出临界区，直到readers归零，便可以顺利进入临界区.
 * 此时不可能有其他读者或者写者再次进入临界区修改readers，因为writer已经被持有。
 */
void rwlock_read_lock(rwlock_t* lock){
    task_t* current = get_current();
    int expected = 0;
    int new_value = 1;
    
    while (true) {
        asm volatile(
            "lock cmpxchg %2, %1"
            : "=a"(expected), "+m"(lock->writer)
            : "r"(new_value), "0"(expected)
            : "memory");

        // 如果cmpxchg成功，则 writer 已经从0变成了1
        if (expected == 0) {
            break;  // 成功获取锁，退出循环
        }
        expected = 0;  // 重置expected为0，用于下次比较
    }
    // readers++
    asm volatile(
        "lock inc %0"
        : "+m"(lock->readers)
        : 
        : "memory");
    // 释放writer
    asm volatile(
        "lock dec %0"
        : "+m"(lock->writer)
        : 
        : "memory");
}
void rwlock_read_unlock(rwlock_t* lock){
    asm volatile(
        "lock dec %0"
        : "+m"(lock->readers)
        : 
        : "memory");
}

void rwlock_write_lock(rwlock_t* lock){
    task_t* current = get_current();
    int expected = 0;
    int new_value = 1;
    
    while (true) {
        asm volatile(
            "lock cmpxchg %2, %1"
            : "=a"(expected), "+m"(lock->writer)
            : "r"(new_value), "0"(expected)
            : "memory");

        // 如果cmpxchg成功，则 writer 已经从0变成了1
        if (expected == 0) {
            break;  // 成功获取锁，退出循环
        }
        expected = 0;  // 重置expected为0，用于下次比较
    }
    // printk("writer get rwlock, waiting for readers leave...\n");
    while (lock->readers != 0); //等待所有读者离开临界区
}

void rwlock_write_unlock(rwlock_t* lock){
    lock->writer = 0; //释放写锁
    // 因为并非采用阻塞队列唤醒的方式，容易一直争抢锁导致读者饿死，需要主动调度
    sys_yield();
}
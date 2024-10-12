#ifndef LIGHTOS_MUTEX_H
#define LIGHTOS_MUTEX_H

#include <sys/types.h>
#include <lib/list.h>
#include <lightos/task.h>
#include <lightos/interrupt.h>

typedef struct mutex_t{
    bool value;
    list_t waiters;
} mutex_t;

// 读写锁
typedef struct rwlock_t{
    bool writer;    // 写者状态
    u32 readers;    // 读者计数
} rwlock_t;

void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);

void rwlock_init(rwlock_t* lock);
void rwlock_read_lock(rwlock_t* lock);
void rwlock_write_lock(rwlock_t* lock);
void rwlock_read_unlock(rwlock_t* lock);
void rwlock_write_unlock(rwlock_t* lock);


#endif
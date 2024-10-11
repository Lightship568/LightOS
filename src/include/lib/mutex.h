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

void mutex_init(mutex_t* mutex);
void mutex_lock(mutex_t* mutex);
void mutex_unlock(mutex_t* mutex);


#endif
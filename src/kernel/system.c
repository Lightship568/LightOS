#include <lightos/task.h>

mode_t sys_umask(mode_t mask) {
    task_t* task = get_current();
    mode_t old = task->umask;
    task->umask = mask & 0777;
    return old;
}
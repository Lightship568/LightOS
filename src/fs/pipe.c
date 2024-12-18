#include <lib/debug.h>
#include <lib/kfifo.h>
#include <lightos/fs.h>
#include <lightos/task.h>
#include <sys/assert.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

int pipe_read(inode_t* inode, char* buf, int count) {
    kfifo_t* fifo = (kfifo_t*)inode->desc;
    int nr = 0;
    while (nr < count) {
        // empty buffer, block reader
        while (kfifo_empty(fifo)) {
            assert(inode->rxwaiter == NULL);
            inode->rxwaiter = get_current();
            task_block(inode->rxwaiter, NULL, TASK_BLOCKED);
        }
        buf[nr++] = kfifo_get(fifo);
        // awake possible writer
        if (inode->txwaiter) {
            task_intr_unblock_no_waiting_list(inode->txwaiter);
            inode->txwaiter = NULL;
        }
    }
    return nr;
}

int pipe_write(inode_t* inode, char* buf, int count) {
    kfifo_t* fifo = (kfifo_t*)inode->desc;
    int nr = 0;
    while (nr < count) {
        // full buffer, block writer
        while (kfifo_full(fifo)) {
            assert(inode->txwaiter == NULL);
            inode->txwaiter = get_current();
            task_block(inode->txwaiter, NULL, TASK_BLOCKED);
        }
        kfifo_put(fifo, buf[nr++]);
        // awake possible reader
        if (inode->rxwaiter) {
            task_intr_unblock_no_waiting_list(inode->rxwaiter);
            inode->rxwaiter = NULL;
        }
    }
    return nr;
}

int32 sys_pipe(fd_t pipefd[2]) {
    inode_t* inode = get_pipe_inode();
    task_t* task = get_current();
    file_t* files[2];

    pipefd[0] = task_get_fd(task);
    files[0] = task->files[pipefd[0]] = get_file();

    pipefd[1] = task_get_fd(task);
    files[1] = task->files[pipefd[1]] = get_file();

    files[0]->inode = inode;
    files[0]->flags = O_RDONLY;

    files[1]->inode = inode;
    files[1]->flags = O_WRONLY;

    return 0;
}
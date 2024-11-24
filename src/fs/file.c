#include <lib/string.h>
#include <lightos/fs.h>
#include <lightos/task.h>
#include <sys/assert.h>

// 全系统最大同时打开文件数
#define FILE_NR 128

file_t file_table[FILE_NR];

file_t* get_file(void) {
    for (size_t i = 3; i < FILE_NR; ++i) {
        file_t* file = &file_table[i];
        if (!file->count) {
            file->count++;
            return file;
        }
    }
    panic("File table out of FILE_NR %d\n", FILE_NR);
}

void put_file(file_t* file) {
    assert(file->count > 0);
    file->count--;
    if (!file->count) {
        iput(file->inode);
    }
}

fd_t sys_open(char* filename, int flags, int mode){
    inode_t* inode = inode_open(filename, flags, mode);
    if (!inode){
        return EOF;
    }
    task_t* task = get_current();
    fd_t fd = task_get_fd(task);
    file_t* file = get_file();
    assert(task->files[fd] == NULL);
    task->files[fd] = file;

    file->inode = inode;
    file->flags = flags;
    file->count = 1;
    file->mode = inode->desc->mode;

    if (flags & O_APPEND){
        file->offset = file->inode->desc->size;
    }else{
        file->offset = 0;
    }
    return fd;
}

fd_t sys_creat(char* filename, int mode){
    return sys_open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd){
    assert(fd >= 3);
    assert(fd < TASK_FILE_NR);
    task_t* task = get_current();
    file_t* file = task->files[fd];
    if (!file){
        return;
    }
    assert(file->inode);
    put_file(file); // --count => iput
    task_put_fd(task, fd);
}

void file_init(void) {
    memset(file_table, 0, sizeof(file_table));
}
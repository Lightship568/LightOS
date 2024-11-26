#include <lib/string.h>
#include <lightos/device.h>
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

fd_t sys_open(char* filename, int flags, int mode) {
    inode_t* inode = inode_open(filename, flags, mode);
    if (!inode) {
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

    if (flags & O_APPEND) {
        file->offset = file->inode->desc->size;
    } else {
        file->offset = 0;
    }
    return fd;
}

fd_t sys_creat(char* filename, int mode) {
    return sys_open(filename, O_WRONLY | O_CREAT | O_TRUNC, mode);
}

void sys_close(fd_t fd) {
    assert(fd >= 3);
    assert(fd < TASK_FILE_NR);
    task_t* task = get_current();
    file_t* file = task->files[fd];
    if (!file) {
        return;
    }
    assert(file->inode);
    put_file(file);  // --count => iput
    task_put_fd(task, fd);
}

int32 sys_read(fd_t fd, char* buf, u32 len) {
    if (fd == stdin) {
        device_t* device = device_find(DEV_KEYBOARD, 0);
        return device_read(device->dev, buf, len, 0, 0);
    } else if (fd == stdout || fd == stderr) {
        return EOF;
    }

    task_t* task = get_current();
    file_t* file = task->files[fd];
    assert(file);
    assert(len > 0);
    // 仅写入，不可读
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        return EOF;
    }
    inode_t* inode = file->inode;
    len = inode_read(inode, buf, len, file->offset);
    // 读取成功，增加文件偏移
    if (len != EOF) {
        file->offset += len;
    }
    return len;
}

int32 sys_write(fd_t fd, char* buf, u32 len) {
    if (fd == stdout || fd == stderr) {
        device_t* device = device_find(DEV_CONSOLE, 0);
        return device_write(device->dev, buf, len, 0, 0);
    } else if (fd == stdin) {
        return EOF;
    }

    task_t* task = get_current();
    file_t* file = task->files[fd];
    assert(file);
    assert(len > 0);
    // 仅读取，不可写
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        return EOF;
    }
    inode_t* inode = file->inode;
    len = inode_write(inode, buf, len, file->offset);
    // 写入成功，增加文件偏移
    if (len != EOF) {
        file->offset += len;
    }
    return len;
}

int32 sys_lseek(fd_t fd, off_t offset, whence_t whence) {
    assert(fd < TASK_FILE_NR);
    task_t* task = get_current();
    file_t* file = task->files[fd];
    int ret = EOF;

    assert(file);
    // 个人认为下面这句没必要，只有打开的文件才会有 file，不需要 inode 判断
    // assert(file->inode);

    switch (whence) {
        case SEEK_SET:
            if (offset < 0) {
                break;
            }
            file->offset = offset;
            ret = file->offset;
            break;
        case SEEK_CUR:
            if (file->offset + offset < 0) {
                break;
            }
            file->offset += offset;
            ret = file->offset;
            break;
        case SEEK_END:
            if (file->inode->desc->size + offset < 0) {
                break;
            }
            file->offset = file->inode->desc->size + offset;
            ret = file->offset;
            break;
    }

    return ret;
}

void file_init(void) {
    memset(file_table, 0, sizeof(file_table));
}
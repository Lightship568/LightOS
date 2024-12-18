#include <lib/arena.h>
#include <lib/stdlib.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/fs.h>
#include <lightos/stat.h>
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
    assert(fd >= 0);
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

int32 sys_read(fd_t fd, char* buf, u32 count) {
    if (fd < 0 || fd >= TASK_FILE_NR) {
        return EOF;
    }
    task_t* task = get_current();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);
    // 仅写入，不可读
    if ((file->flags & O_ACCMODE) == O_WRONLY) {
        goto exit;
    }

    inode_t* inode = file->inode;
    int ret = EOF;

    // 管道文件判断
    if (inode->pipe){
        ret = pipe_read(inode, buf, count);
        goto exit;
    }
    // 字符设备、块设备、与正常文件判断
    else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        ret = device_read(inode->desc->zone[0], buf, count, 0, 0);
        goto exit;
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        device_t* device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SECS == 0);
        assert(count % BLOCK_SIZE == 0);
        ret = device_read(inode->desc->zone[0], buf, count / BLOCK_SIZE,
                          file->offset / BLOCK_SIZE, 0);
        goto exit;
    } else {
        ret = inode_read(inode, buf, count, file->offset);
        // 读取成功，增加文件偏移
        if (ret != EOF) {
            file->offset += ret;
        }
        goto exit;
    }
exit:
    return ret;
}

int32 sys_write(fd_t fd, char* buf, u32 count) {
    if (fd <= 0 || fd >= TASK_FILE_NR) {
        return EOF;
    }
    task_t* task = get_current();
    file_t* file = task->files[fd];
    assert(file);
    assert(count > 0);
    // 仅读取，不可写
    if ((file->flags & O_ACCMODE) == O_RDONLY) {
        return EOF;
    }

    inode_t* inode = file->inode;
    int ret = EOF;
    // 管道文件判断
    if (inode->pipe){
        ret = pipe_write(inode, buf, count);
        goto exit;
    }
    // 字符设备、块设备、与正常文件判断
    else if (ISCHR(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        ret = device_write(inode->desc->zone[0], buf, count, 0, 0);
        goto exit;
    } else if (ISBLK(inode->desc->mode)) {
        assert(inode->desc->zone[0]);
        device_t* device = device_get(inode->desc->zone[0]);
        assert(file->offset % BLOCK_SECS == 0);
        assert(count % BLOCK_SIZE == 0);
        ret = device_write(inode->desc->zone[0], buf, count / BLOCK_SIZE,
                          file->offset / BLOCK_SIZE, 0);
        goto exit;
    } else {
        ret = inode_write(inode, buf, count, file->offset);
        // 读取成功，增加文件偏移
        if (ret != EOF) {
            file->offset += ret;
        }
        goto exit;
    }
exit:
    return ret;
}

int32 sys_lseek(fd_t fd, off_t offset, whence_t whence) {
    if (fd <= STDERR_FILENO || fd >= TASK_FILE_NR) {
        return EOF;
    }
    task_t* task = get_current();
    file_t* file = task->files[fd];
    int ret = EOF;

    if (!file) {
        return EOF;
    }
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

int32 sys_getcwd(char* buf, size_t size) {
    task_t* task = get_current();
    int len = MIN(task->pwd_len + 1, size);
    strncpy(buf, task->pwd, len);
    return len;
}

// 处理复杂路径字符串，并转换为绝对路径
static char* abspath(char* pathname, u32 pathname_len) {
    task_t* task = get_current();
    char* tmpstr;
    // 相对路径，将绝对路径补齐
    if (!IS_SEPARATOR(pathname[0])) {
        pathname_len = task->pwd_len + pathname_len + 3;  // 补充 2*'/' + EOS
        tmpstr = kmalloc(pathname_len);
        strcpy(tmpstr, task->pwd);
        tmpstr[task->pwd_len] = SEPARATOR1;
        strcpy(&tmpstr[task->pwd_len + 1], pathname);
        tmpstr[pathname_len - 2] = SEPARATOR1;
        tmpstr[pathname_len - 1] = EOS;
    }
    // 绝对路径，直接处理
    else {
        pathname_len += 2;
        tmpstr = kmalloc(pathname_len);
        strcpy(tmpstr, pathname);
        tmpstr[pathname_len - 2] = SEPARATOR1;
        tmpstr[pathname_len - 1] = EOS;
    }
    assert(IS_SEPARATOR(tmpstr[0]));
    char* t = tmpstr;  // truth
    char* c = tmpstr;  // current
    while (c[1] != EOS) {
        if (IS_SEPARATOR(c[1])) {
            c++;
        } else if (c[1] == '.' && IS_SEPARATOR(c[2])) {
            c += 2;
        } else if (c[1] == '.' && c[2] == '.' && IS_SEPARATOR(c[3])) {
            c += 3;
            while (t > tmpstr && !IS_SEPARATOR(*(--t)))
                ;
        } else {
            while (!IS_SEPARATOR(c[1])) {
                *(++t) = *(++c);
            }
            *(++t) = SEPARATOR1;
        }
    }
    if (t > tmpstr) {
        *t = EOS;
    } else {
        *(++t) = EOS;
    }
    return tmpstr;
}

int32 sys_chdir(char* pathname) {
    u32 pathname_len = strlen(pathname);
    assert(pathname_len < MAX_PATH_LEN);
    if (pathname_len < 1) {  // 至少一个字符
        return EOF;
    }

    int ret = 0;
    // todo: 若传入相对路径，可以优化 namei 相对路径查询更快
    char* tmpstr = abspath(pathname, pathname_len);
    pathname_len = strlen(tmpstr);

    task_t* task = get_current();
    inode_t* inode = namei(tmpstr);
    if (!inode) {
        ret = EOF;
        goto clean;
    }
    if (!ISDIR(inode->desc->mode)) {
        ret = EOF;
        goto clean;
    }
    // 本目录无需切换
    if (inode == task->ipwd) {
        goto clean;
    }

    iput(task->ipwd);
    task->ipwd = inode;
    task->pwd_len = pathname_len;
    // 申请更长的 pwd
    if (task->pwd_len < pathname_len) {
        kfree(task->pwd);
        task->pwd = kmalloc(pathname_len);
    }

    memcpy(task->pwd, tmpstr, pathname_len);
    kfree(tmpstr);
    return 0;

clean:
    kfree(tmpstr);
    iput(inode);
    return ret;
}
int32 sys_chroot(char* pathname) {
    u32 pathname_len = strlen(pathname);
    assert(pathname_len < MAX_PATH_LEN);
    if (pathname_len < 1) {  // 至少一个字符
        return EOF;
    }

    int ret = 0;
    char* tmpstr = abspath(pathname, pathname_len);
    pathname_len = strlen(tmpstr);

    task_t* task = get_current();
    inode_t* inode = namei(tmpstr);
    kfree(tmpstr);
    if (!inode) {
        ret = EOF;
        goto clean;
    }
    if (!ISDIR(inode->desc->mode) || inode == task->ipwd) {
        ret = EOF;
        goto clean;
    }

    iput(task->iroot);
    task->iroot = inode;
    return ret;

clean:
    iput(inode);
    return ret;
}

int32 sys_readdir(fd_t fd, void* dir, int count) {
    return sys_read(fd, (char*)dir, count);
}


static int dupfd(fd_t fd, fd_t arg){
    task_t* task = get_current();
    if (fd >= TASK_FILE_NR || !task->files[fd]){
        return EOF;
    }
    // 找空 fd
    for (; arg < TASK_FILE_NR; arg++){
        if (!task->files[arg]){
            break;
        }
    }
    // 打开文件到达上限
    if (arg >= TASK_FILE_NR){
        return EOF;
    }
    // 拷贝指针，并增加原文件引用计数
    task->files[arg] = task->files[fd];
    task->files[fd]->count++;
    return arg;
}

fd_t sys_dup(fd_t oldfd){
    return dupfd(oldfd, 0);
}

fd_t sys_dup2(fd_t oldfd, fd_t newfd){
    // 合法操作，但健壮的 shell 应该不会发生
    if (oldfd == newfd){
        return newfd;
    }
    sys_close(newfd);
    return dupfd(oldfd, newfd);
}

void file_init(void) {
    for (size_t i = 3; i > FILE_NR; +i) {
        file_t* file = &file_table[i];
        file->mode = 0;
        file->count = 0;
        file->flags = 0;
        file->offset = 0;
        file->inode = NULL;
    }
}
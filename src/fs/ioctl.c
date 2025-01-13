#include <lightos/device.h>
#include <lightos/fs.h>
#include <sys/errno.h>

int sys_ioctl(fd_t fd, int cmd, void* args){
    if (fd < 0 || fd >= TASK_FILE_NR){
        return -EBADF;
    }
    task_t* task = get_current();
    file_t* file = task->files[fd];
    if (!file){
        return -EBADF;
    }

    // 文件必须是某种（字符或块）设备
    int mode = file->inode->desc->mode;
    if (!ISCHR(mode) && !ISBLK(mode)){
        return -EINVAL;
    }

    // 得到设备号
    dev_t dev = file->inode->desc->zone[0];
    if (dev >= DEVICE_NR){
        return -ENOTTY;
    }

    // 进行设备控制
    return device_ioctl(dev, cmd, args, 0);

}
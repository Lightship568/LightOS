#include <lightos/fs.h>
#include <lightos/stat.h>
#include <sys/assert.h>


static void copy_stat(inode_t* inode, stat_t* statbuf){
    statbuf->dev = inode->dev;
    statbuf->nr = inode->nr;
    statbuf->mode = inode->desc->mode;
    statbuf->nlinks = inode->desc->nlinks;
    statbuf->uid = inode->desc->uid;
    statbuf->gid = inode->desc->gid;
    statbuf->rdev = inode->desc->zone[0];
    statbuf->size = inode->desc->size;
    statbuf->atime = inode->atime;
    statbuf->mtime = inode->desc->mtime;
    statbuf->ctime = inode->ctime;
}

u32 sys_stat(char* filename, stat_t* statbuf){
    inode_t* inode = namei(filename);
    if (!inode){
        return EOF;
    }
    copy_stat(inode, statbuf);
    iput(inode);
    return 0;
}

u32 sys_fstat(fd_t fd, stat_t* statbuf){
    if (fd < 0 || fd >= TASK_FILE_NR){
        return EOF;
    }
    // todo 标准流虚拟设备
    task_t* task = get_current();
    file_t* file = task->files[fd];
    if (!file){
        return EOF;
    }
    inode_t* inode = file->inode;
    assert(inode);

    copy_stat(inode, statbuf);
    return 0;
}
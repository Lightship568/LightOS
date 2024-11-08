#include <lightos/fs.h>
#include <lightos/device.h>
#include <lib/string.h>
#include <lib/debug.h>
#include <sys/assert.h>


#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

/**
 * MINIX V1，每块 1K，顺序如下
 * | boot | superblock | imap(s) | bmap(s) | inode(s)...
 */
#define FS_BOOT_BLOCK_NR 0
#define FS_SUPER_BLOCK_NR 1
#define FS_IMAP_BLOCK_NR 2

void super_init(void){
    device_t* device = device_find(DEV_IDE_PART, 0);
    assert(device);

    cache_t* boot = bread(device->dev, FS_BOOT_BLOCK_NR);
    cache_t* super = bread(device->dev, FS_SUPER_BLOCK_NR);

    super_desc_t* sb = (super_desc_t*)super->data;
    assert(sb->magic == MINIX_V1_MAGIC);

    // inode 位图
    cache_t* imap = bread(device->dev, FS_IMAP_BLOCK_NR);
    // 块位图
    cache_t* zmap = bread(device->dev, FS_IMAP_BLOCK_NR + sb->imap_blocks);

    // 读取首个 inode 块
    cache_t* c1 = bread(device->dev, FS_IMAP_BLOCK_NR + sb->imap_blocks + sb->zmap_blocks);
    inode_desc_t* inode = (inode_desc_t*)c1->data;

    // 找到首个数据块，作为dir进行读取
    cache_t* c2 = bread(device->dev, inode->zone[0]);
    dentry_t* dir = (dentry_t*)c2->data;

    inode_desc_t* helloi = NULL;
    while(dir->nr){
        LOGK("inode %04d, name %s\n", dir->nr, dir->name);
        if(!strcmp(dir->name, "hello.txt")){
            // 找到hello.txt的inode节点
            helloi = &inode[dir->nr - 1];
            strcpy(dir->name, "world.txt");
            c2->dirty = true;
            bwrite(c2); // file name
        }
        dir++;
    }
    if (helloi){
        cache_t* c3 = bread(device->dev, helloi->zone[0]);
        LOGK("content: %s", c3->data);

        strncpy(c3->data, "This is modified content!!!\n", 29);
        c3->dirty = true;

        helloi->size = strlen(c3->data);
        c1->dirty = true;
        bwrite(c1); // file size
        bwrite(c3); // file content
    }
    
    
    
}
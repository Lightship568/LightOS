#include <lightos/fs.h>
#include <lightos/device.h>
#include <lightos/stat.h>
#include <lib/stdio.h>
#include <lib/vsprintf.h>
#include <sys/assert.h>

void dev_init(void){
    sys_mkdir("/dev", 0755);
    device_t* device = NULL;

    // 初始化第一个 ramdisk 的文件系统作为 /dev
    device = device_find(DEV_RAMDISK, 0);
    assert(device);
    devmkfs(device->dev, 0);

    // 手动 mount 到 /dev
    super_block_t* sb = read_super(device->dev);
    sb->iroot = iget(device->dev, 1);
    sb->imount = namei("/dev");
    sb->imount->mount = device->dev;

    // 终端与键盘
    device = device_find(DEV_CONSOLE, 0);
    sys_mknod("/dev/console", IFCHR | IWUSR, device->dev);

    device = device_find(DEV_KEYBOARD, 0);
    sys_mknod("/dev/keyboard", IFCHR | IRUSR, device->dev);

    // 列出所有块设备
    char name[NAMELEN];
    u32 subtype_list[] = {DEV_IDE_DISK, DEV_IDE_PART, DEV_RAMDISK};
    size_t target = 0;
    size_t i = 0;
    for (;;){
        device = device_find(subtype_list[target], i++);
        if (!device){
            if (++target > sizeof(subtype_list) / sizeof(u32)){
                break;
            }
            i = 0;
            continue;
        }
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | (IRUSR | IWUSR), device->dev);
    }
}
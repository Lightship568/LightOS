#include <lightos/fs.h>
#include <lightos/device.h>
#include <lightos/stat.h>
#include <lib/stdio.h>
#include <lib/vsprintf.h>

void dev_init(void){
    sys_mkdir("/dev", 0755);
    device_t* device = NULL;
    device = device_find(DEV_CONSOLE, 0);
    sys_mknod("/dev/console", IFCHR | IWUSR, device->dev);

    device = device_find(DEV_KEYBOARD, 0);
    sys_mknod("/dev/keyboard", IFCHR | IRUSR, device->dev);

    char name[NAMELEN];

    for (size_t i = 0; true; i++){
        device = device_find(DEV_IDE_DISK, i);
        if (!device){
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | (IRUSR | IWUSR), device->dev);
    }

    for (size_t i = 0; true; i++){
        device = device_find(DEV_IDE_PART, i);
        if (!device){
            break;
        }
        sprintf(name, "/dev/%s", device->name);
        sys_mknod(name, IFBLK | (IRUSR | IWUSR), device->dev);
    }
}
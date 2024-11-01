#include <lib/arena.h>
#include <lib/debug.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/task.h>
#include <sys/assert.h>

#define DEVICE_NR 64  // 设备数量

static device_t device_list[DEVICE_NR];

// 获取新的空设备槽位
static device_t* get_new_device() {
    for (size_t i = 1; i < DEVICE_NR; i++) {
        device_t* device = &device_list[i];
        if (device->type == DEV_NULL)
            return device;
    }
    panic("Too much devices out of %d!\n", DEVICE_NR);
}

device_t* device_find(int subtype, idx_t idx){
    idx_t nr = 0;
    device_t* device;
    for (size_t i = 0; i < DEVICE_NR; i++){
        device = &device_list[i];
        if (device->subtype == subtype){
            if (nr++ == idx){
                return device;
            }
        }
    }
    return NULL;
}

device_t* device_get(dev_t dev){
    assert(dev < DEVICE_NR);
    assert(device_list[dev].type != DEV_NULL);
    return &device_list[dev];
}

int device_ioctl(dev_t dev, int cmd, void* args, int flags) {
    device_t* device = device_get(dev);
    if (device->ioctl) {
        return device->ioctl(device->ptr, cmd, args, flags);
    }
    DEBUGK("Device %s(id %d) IOCTL is not implemented\n", device->name, dev);
    return EOF;
}

int device_read(dev_t dev, void* buf, size_t count, idx_t idx, int flags) {
    device_t* device = device_get(dev);
    if (device->read) {
        return device->read(device->ptr, buf, count, idx, flags);
    }
    DEBUGK("Device %s(id %d) READ is not implemented\n", device->name, dev);
    return EOF;
}

int device_write(dev_t dev, void* buf, size_t count, idx_t idx, int flags) {
    device_t* device = device_get(dev);
    if (device->write) {
        return device->write(device->ptr, buf, count, idx, flags);
    }
    assert(device->type != DEV_CONSOLE); // 非 console
    DEBUGK("Device %s(id %d) WRITE is not implemented\n", device->name, dev);
    return EOF;
}

dev_t device_install(int type,
                     int subtype,
                     void* ptr,
                     char* name,
                     dev_t parent,
                     void* ioctl,
                     void* read,
                     void* write) {

    device_t* device = get_new_device();

    device->type = type;
    device->subtype = subtype;
    device->ptr = ptr;
    strncpy(device->name, name, NAMELEN);
    device->parent = parent;
    
    device->ioctl = ioctl;
    device->read = read;
    device->write = write;

    return device->dev;
}

void device_init() {
    for (size_t i = 0; i < DEVICE_NR; ++i) {
        device_t* device = &device_list[i];
        strcpy((char*)device->name, "null");
        device->type = DEV_NULL;
        device->subtype = DEV_NULL;
        device->dev = i;
        device->parent = 0;
        device->ioctl = NULL;
        device->read = NULL;
        device->write = NULL;
    }
}

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
        list_init(&device->request_list);
    }
}

static void do_request(request_t* req){
    switch (req->type) {
    case REQUEST_READ:
        device_read(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    case REQUEST_WRITE:
        device_write(req->dev, req->buf, req->count, req->idx, req->flags);
        break;
    default:
        panic("Request type 0x%x undefined error\n", req->type);
        break;
    }
}

void device_request(dev_t dev, void* buf, u8 count, idx_t idx, int flags, u32 type){
    device_t* device = device_get(dev);
    // 保证是块设备
    assert(device->type = DEV_BLOCK);

    if (device->parent){ // 找到父设备，也就是根盘，为了管理 req 队列
        device =  device_get(device->parent);
    }

    request_t* req = kmalloc(sizeof(request_t));
    req->dev = dev;
    req->buf = buf;
    req->count = count;
    req->idx = idx;
    req->flags = flags;
    req->type = type;
    req->task = get_current();

    bool empty = list_empty(&device->request_list);

    // 将请求压入对应设备的请求链表
    list_push(&device->request_list, &req->node);

    // 若有请求正在等待处理，则阻塞自己
    if (!empty){
        task_block(req->task, NULL, TASK_BLOCKED);
    }

    do_request(req);

    list_remove(&req->node);

    kfree(req);
    req = NULL;

    // 暂时 FIFO，恢复下一个 request 的任务
    if (!list_empty(&device->request_list)){
        req = element_entry(request_t, node, device->request_list.head.next);
        assert(req->task->magic == LIGHTOS_MAGIC);
        task_intr_unblock_no_waiting_list(req->task);
    }

}

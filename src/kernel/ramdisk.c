#include <lib/debug.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <lightos/memory.h>
#include <sys/assert.h>
#include <lib/vsprintf.h>

#define SECOTR_SIZE 512

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...) ;

#define RAMDISK_NR 4

typedef struct ramdisk_t {
    u8* start;  // 内存开始位置
    u32 size;   // 占用内存大小
} ramdisk_t;

static ramdisk_t ramdisks[RAMDISK_NR];

int ramdisk_ioctl(ramdisk_t* disk, int cmd, void* args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return 0;
        case DEV_CMD_SECTOR_COUNT:
            return disk->size / SECOTR_SIZE;
        default:
            panic("Device ioctl command %d undefined error\n", cmd);
            break;
    }
}

int ramdisk_read(ramdisk_t* disk, void* buf, u8 count, idx_t lba) {
    void* addr = disk->start + lba * SECOTR_SIZE;
    u32 len = count * SECOTR_SIZE;
    if (((u32)addr + len) >= (KERNEL_RAMDISK_VADDR + KERNEL_RAMDISK_SIZE)) {
        panic("Trying to read out of ramdisk realm from 0x%p with len 0x%x\n",
              addr, len);
    }
    memcpy(buf, addr, len);
    return count;
}

int ramdisk_write(ramdisk_t* disk, void* buf, u8 count, idx_t lba) {
    void* addr = disk->start + lba * SECOTR_SIZE;
    u32 len = count * SECOTR_SIZE;
    if (((u32)addr + len) >= (KERNEL_RAMDISK_VADDR + KERNEL_RAMDISK_SIZE)) {
        panic("Trying to write out of ramdisk realm from 0x%p with len 0x%x\n",
              addr, len);
    }
    memcpy(addr, buf, len);
    return count;
}

void ramdisk_init(void) {
    u32 size = KERNEL_RAMDISK_SIZE / RAMDISK_NR;
    assert(size % SECOTR_SIZE == 0);
    char name[32];
    for (size_t i = 0; i < RAMDISK_NR; ++i) {
        ramdisk_t* ramdisk = &ramdisks[i];
        ramdisk->start = (u8*)(KERNEL_RAMDISK_VADDR + size * i);
        ramdisk->size = size;
        sprintf(name, "md%c", i + 'a');
        device_install(DEV_BLOCK, DEV_RAMDISK, ramdisk, name, 0, ramdisk_ioctl,
                       ramdisk_read, ramdisk_write);
    }
    LOGK("Ramdisk initalized\n");
}
#include <lib/debug.h>
#include <lib/io.h>
#include <lib/print.h>
#include <lib/stdio.h>
#include <lib/string.h>
#include <lib/vsprintf.h>
#include <lightos/device.h>
#include <lightos/ide.h>
#include <lightos/interrupt.h>
#include <lightos/memory.h>
#include <lightos/task.h>
#include <sys/assert.h>

// #define LOGK(fmt, args...) DEBUGK(fmt, ##args)
#define LOGK(fmt, args...) ;

#define EOK 0
#define EIO 1
#define ETIME 2

#define IDE_TIMEOUT 60000

// IDE 寄存器基址，是 Port I/O 地址，不是 MMIO，不需要增加虚拟地址偏移
#define IDE_IOBASE_PRIMARY 0x1F0    // 主通道基地址
#define IDE_IOBASE_SECONDARY 0x170  // 从通道基地址

// IDE 寄存器偏移
#define IDE_DATA 0x0000        // 数据寄存器
#define IDE_ERR 0x0001         // 错误寄存器
#define IDE_FEATURE 0x0001     // 功能寄存器
#define IDE_SECTOR 0x0002      // 扇区数量
#define IDE_LBA_LOW 0x0003     // LBA 低字节
#define IDE_CHS_SECTOR 0x0003  // CHS 扇区位置
#define IDE_LBA_MID 0x0004     // LBA 中字节
#define IDE_CHS_CYL 0x0004     // CHS 柱面低字节
#define IDE_LBA_HIGH 0x0005    // LBA 高字节
#define IDE_CHS_CYH 0x0005     // CHS 柱面高字节
#define IDE_HDDEVSEL 0x0006    // 磁盘选择寄存器
#define IDE_STATUS 0x0007      // 状态寄存器
#define IDE_COMMAND 0x0007     // 命令寄存器
#define IDE_ALT_STATUS 0x0206  // 备用状态寄存器
#define IDE_CONTROL 0x0206     // 设备控制寄存器
#define IDE_DEVCTRL 0x0206     // 驱动器地址寄存器

// IDE 命令

#define IDE_CMD_READ 0x20        // 读命令
#define IDE_CMD_WRITE 0x30       // 写命令
#define IDE_CMD_IDENTIFY 0xEC    // 识别命令
#define IDE_CMD_DIAGNOSTIC 0x90  // 诊断命令

#define IDE_CMD_READ_UDMA 0xC8   // UDMA 读命令
#define IDE_CMD_WRITE_UDMA 0xCA  // UDMA 写命令

#define IDE_CMD_PIDENTIFY 0xA1  // 识别 PACKET 命令
#define IDE_CMD_PACKET 0xA0     // PACKET 命令

// ATAPI 命令
#define IDE_ATAPI_CMD_REQUESTSENSE 0x03
#define IDE_ATAPI_CMD_READCAPICITY 0x25
#define IDE_ATAPI_CMD_READ10 0x28

#define IDE_ATAPI_FEATURE_PIO 0
#define IDE_ATAPI_FEATURE_DMA 1

// IDE 控制器状态寄存器
#define IDE_SR_NULL 0x00  // NULL
#define IDE_SR_ERR 0x01   // Error
#define IDE_SR_IDX 0x02   // Index
#define IDE_SR_CORR 0x04  // Corrected data
#define IDE_SR_DRQ 0x08   // Data request
#define IDE_SR_DSC 0x10   // Drive seek complete
#define IDE_SR_DWF 0x20   // Drive write fault
#define IDE_SR_DRDY 0x40  // Drive ready
#define IDE_SR_BSY 0x80   // Controller busy

// IDE 控制寄存器
#define IDE_CTRL_HD15 0x00  // Use 4 bits for head (not used, was 0x08)
#define IDE_CTRL_SRST 0x04  // Soft reset
#define IDE_CTRL_NIEN 0x02  // Disable interrupts

// IDE 错误寄存器
#define IDE_ER_AMNF 0x01   // Address mark not found
#define IDE_ER_TK0NF 0x02  // Track 0 not found
#define IDE_ER_ABRT 0x04   // Abort
#define IDE_ER_MCR 0x08    // Media change requested
#define IDE_ER_IDNF 0x10   // Sector id not found
#define IDE_ER_MC 0x20     // Media change
#define IDE_ER_UNC 0x40    // Uncorrectable data error
#define IDE_ER_BBK 0x80    // Bad block

#define IDE_LBA_MASTER 0b11100000  // 主盘 LBA
#define IDE_LBA_SLAVE 0b11110000   // 从盘 LBA
#define IDE_SEL_MASK 0b10110000    // CHS 模式 MASK

#define IDE_INTERFACE_UNKNOWN 0
#define IDE_INTERFACE_ATA 1
#define IDE_INTERFACE_ATAPI 2

// 总线主控寄存器偏移
#define BM_COMMAND_REG 0  // 命令寄存器偏移
#define BM_STATUS_REG 2   // 状态寄存器偏移
#define BM_PRD_ADDR 4     // PRD 地址寄存器偏移

// 总线主控命令寄存器
#define BM_CR_STOP 0x00   // 终止传输
#define BM_CR_START 0x01  // 开始传输
#define BM_CR_WRITE 0x00  // 主控写磁盘
#define BM_CR_READ 0x08   // 主控读磁盘

// 总线主控状态寄存器
#define BM_SR_ACT 0x01      // 激活
#define BM_SR_ERR 0x02      // 错误
#define BM_SR_INT 0x04      // 中断信号生成
#define BM_SR_DRV0 0x20     // 驱动器 0 可以使用 DMA 方式
#define BM_SR_DRV1 0x40     // 驱动器 1 可以使用 DMA 方式
#define BM_SR_SIMPLEX 0x80  // 仅单纯形操作

#define IDE_LAST_PRD 0x80000000  // 最后一个描述符

// 分区文件系统
// 参考 https://www.win.tue.nl/~aeb/partitions/partition_types-1.html
typedef enum PART_FS {
    PART_FS_FAT12 = 1,     // FAT12
    PART_FS_EXTENDED = 5,  // 扩展分区
    PART_FS_MINIX = 0x80,  // minux
    PART_FS_LINUX = 0x83,  // linux
} PART_FS;

typedef struct ide_params_t {
    u16 config;                  // 0 General configuration bits
    u16 cylinders;               // 01 cylinders
    u16 RESERVED;                // 02
    u16 heads;                   // 03 heads
    u16 RESERVED[5 - 3];         // 05
    u16 sectors;                 // 06 sectors per track
    u16 RESERVED[9 - 6];         // 09
    u8 serial[20];               // 10 ~ 19 序列号
    u16 RESERVED[22 - 19];       // 10 ~ 22
    u8 firmware[8];              // 23 ~ 26 固件版本
    u8 model[40];                // 27 ~ 46 模型数
    u8 drq_sectors;              // 47 扇区数量
    u8 RESERVED[3];              // 48
    u16 capabilities;            // 49 能力
    u16 RESERVED[59 - 49];       // 50 ~ 59
    u32 total_lba;               // 60 ~ 61
    u16 RESERVED;                // 62
    u16 mdma_mode;               // 63
    u8 RESERVED;                 // 64
    u8 pio_mode;                 // 64
    u16 RESERVED[79 - 64];       // 65 ~ 79 参见 ATA specification
    u16 major_version;           // 80 主版本
    u16 minor_version;           // 81 副版本
    u16 commmand_sets[87 - 81];  // 82 ~ 87 支持的命令集
    u16 RESERVED[118 - 87];      // 88 ~ 118
    u16 support_settings;        // 119
    u16 enable_settings;         // 120
    u16 RESERVED[221 - 120];     // 221
    u16 transport_major;         // 222
    u16 transport_minor;         // 223
    u16 RESERVED[254 - 223];     // 254
    u16 integrity;               // 校验和
} _packed ide_params_t;

ide_ctrl_t controllers[IDE_CTRL_NR];

static int ide_reset_controller(ide_ctrl_t* ctrl);

// 硬盘中断处理函数
static void ide_handler(int vector) {
    send_eoi(vector);  // 向中断控制器发送中断处理结束信号

    // 得到中断向量对应的控制器
    ide_ctrl_t* ctrl = &controllers[vector - IRQ_HARDDISK - 0x20];

    // 读取常规状态寄存器，表示中断处理结束
    u8 state = inb(ctrl->iobase + IDE_STATUS);
    // LOGK("Harddisk interrupt vector %d state 0x%x\n", vector, state);
    if (ctrl->waiter) {
        // 如果有进程阻塞，则取消阻塞
        task_intr_unblock_no_waiting_list(ctrl->waiter);
        ctrl->waiter = NULL;
    }
}

static void ide_error(ide_ctrl_t* ctrl) {
    u8 error = inb(ctrl->iobase + IDE_ERR);
    if (error & IDE_ER_BBK)
        LOGK("bad block\n");
    if (error & IDE_ER_UNC)
        LOGK("uncorrectable data\n");
    if (error & IDE_ER_MC)
        LOGK("media change\n");
    if (error & IDE_ER_IDNF)
        LOGK("id not found\n");
    if (error & IDE_ER_MCR)
        LOGK("media change requested\n");
    if (error & IDE_ER_ABRT)
        LOGK("abort\n");
    if (error & IDE_ER_TK0NF)
        LOGK("track 0 not found\n");
    if (error & IDE_ER_AMNF)
        LOGK("address mark not found\n");
}

// 硬盘延迟
static void ide_delay() {
    sys_sleep(25);
}

static int32 ide_busy_wait(ide_ctrl_t* ctrl, u8 mask) {  // 同步pio
    while (true) {
        u8 state = inb(
            ctrl->iobase +
            IDE_ALT_STATUS);  // 这里读备用是因为，如果读了常规状态寄存器，则IDE中断不会触发
        if (state & IDE_SR_ERR) {
            ide_error(ctrl);
        }
        if (state & IDE_SR_BSY) {  // 驱动器忙
            continue;
        }
        if ((state & mask) == mask)  // 等待的状态完成
            return EOK;
    }
}

// 重置硬盘控制器
static int32 ide_reset_controller(ide_ctrl_t* ctrl) {
    outb(ctrl->iobase + IDE_CONTROL, IDE_CTRL_SRST);
    ide_delay();
    outb(ctrl->iobase + IDE_CONTROL, ctrl->control);
    return ide_busy_wait(ctrl, IDE_SR_NULL);
}

// 选择磁盘
static void ide_select_drive(ide_disk_t* disk) {
    outb(disk->ctrl->iobase + IDE_HDDEVSEL, disk->selector);
    disk->ctrl->active = disk;
}

// 选择扇区
static void ide_select_sector(ide_disk_t* disk, u32 lba, u8 count) {
    // 输出功能，可省略
    // outb(disk->ctrl->iobase + IDE_FEATURE, 0);

    /** todo: debug
     * PIO 传入的LBA存在-1的偏移？
     * 传入0会abort，传入1则会读写LBA==0的扇区
     */

    // 读写扇区数量
    outb(disk->ctrl->iobase + IDE_SECTOR, count);

    // 28 位 PIO 模式
    // LBA 低字节
    outb(disk->ctrl->iobase + IDE_LBA_LOW, lba & 0xff);
    // LBA 中字节
    outb(disk->ctrl->iobase + IDE_LBA_MID, (lba >> 8) & 0xff);
    // LBA 高字节
    outb(disk->ctrl->iobase + IDE_LBA_HIGH, (lba >> 16) & 0xff);

    // LBA 最高四位 + 磁盘选择
    outb(disk->ctrl->iobase + IDE_HDDEVSEL,
         ((lba >> 24) & 0xf) | disk->selector);

    disk->ctrl->active = disk;
}

// 从磁盘读取一个扇区到 buf
static void ide_pio_read_sector(ide_disk_t* disk, u16* buf) {
    for (size_t i = 0; i < (disk->sector_size / 2); i++) {
        buf[i] = inw(disk->ctrl->iobase + IDE_DATA);
    }
}

// 从 buf 写入一个扇区到磁盘
static void ide_pio_write_sector(ide_disk_t* disk, u16* buf) {
    for (size_t i = 0; i < (disk->sector_size / 2); i++) {
        outw(disk->ctrl->iobase + IDE_DATA, buf[i]);
    }
}

// 硬盘控制
int ide_pio_ioctl(ide_disk_t* disk, int cmd, void* args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return 0;
        case DEV_CMD_SECTOR_COUNT:
            return disk->total_lba;
        default:
            panic("Device ioctl command %d undefined error\n", cmd);
            break;
    }
}

// PIO 方式读取磁盘
int ide_pio_read(ide_disk_t* disk, void* buf, u8 count, idx_t lba) {
    assert(count > 0);
    assert(!get_interrupt_state());  // 异步方式，调用该函数时不许中断

    ide_ctrl_t* ctrl = disk->ctrl;

    mutex_lock(
        &ctrl->lock);  // idle初始化也可以加锁，此时一定可以成功获取不会阻塞

    int ret = -EIO;

    LOGK("Read lba 0x%x\n", lba);

    // 选择磁盘
    ide_select_drive(disk);

    // 等待就绪
    ide_busy_wait(ctrl, IDE_SR_DRDY);

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 发送读命令
    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_READ);

    task_t* task = get_current();
    for (size_t i = 0; i < count; i++) {
        if (task->pid != 0) {  // idle进程即系统初始化过程的读盘无法异步
            ctrl->waiter = task;
            task_block(
                task, NULL,
                TASK_BLOCKED);  // 阻塞自己等待中断的到来，等待磁盘准备数据
        }
        ide_busy_wait(ctrl, IDE_SR_DRQ);
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_read_sector(disk, (u16*)offset);
    }
    ret = EOK;

rollback:
    mutex_unlock(&ctrl->lock);
    return ret;
}

// PIO 方式写磁盘
int ide_pio_write(ide_disk_t* disk, void* buf, u8 count, idx_t lba) {
    assert(count > 0);
    assert(!get_interrupt_state());  // 异步方式，调用该函数时不许中断

    ide_ctrl_t* ctrl = disk->ctrl;

    mutex_lock(&ctrl->lock);

    int ret = EOK;

    LOGK("Write lba 0x%x\n", lba);

    // 选择磁盘
    ide_select_drive(disk);

    // 等待就绪
    ide_busy_wait(ctrl, IDE_SR_DRDY);

    // 选择扇区
    ide_select_sector(disk, lba, count);

    // 发送写命令
    outb(ctrl->iobase + IDE_COMMAND, IDE_CMD_WRITE);
    task_t* task = get_current();
    for (size_t i = 0; i < count; i++) {
        u32 offset = ((u32)buf + i * SECTOR_SIZE);
        ide_pio_write_sector(disk, (u16*)offset);

        if (task->pid != 0) {  // idle进程即系统初始化过程的写盘无法异步
            ctrl->waiter = task;
            task_block(
                task, NULL,
                TASK_BLOCKED);  // 阻塞自己等待中断的到来，等待磁盘准备数据
        }

        ide_busy_wait(ctrl, IDE_SR_NULL);
    }
    ret = EOK;

rollback:
    mutex_unlock(&ctrl->lock);
    return ret;
}

// 分区控制
int ide_pio_part_ioctl(ide_part_t* part, int cmd, void* args, int flags) {
    switch (cmd) {
        case DEV_CMD_SECTOR_START:
            return part->start;
        case DEV_CMD_SECTOR_COUNT:
            return part->count;
        default:
            panic("Device ioctl command %d undefined error\n", cmd);
            break;
    }
}


// 读分区
int ide_pio_part_read(ide_part_t* part, void* buf, u8 count, idx_t lba) {
    return ide_pio_read(part->disk, buf, count, part->start + lba);
}

// 写分区
int ide_pio_part_write(ide_part_t* part, void* buf, u8 count, idx_t lba) {
    return ide_pio_write(part->disk, buf, count, part->start + lba);
}

static void ide_part_init(ide_disk_t* disk, u16* buf) {
    boot_sector_t* boot;  // MBR 起始扇区
    part_entry_t* entry;  // 分区表项
    ide_part_t* part;     // disk中的分区表项信息

    boot_sector_t* eboot;  // 扩展分区起始扇区
    part_entry_t* eentry;  // 扩展分区表项

    LOGK("Checking Disk %s Partition Information\n", disk->name);

    // 读取MBR扇区
    ide_pio_read(disk, buf, 1, 0);

    boot = (boot_sector_t*)buf;

    // 防止vmware的IDE不是MBR
    if (boot->signature != 0xaa55) {
        LOGK("Disk %s isn't MBR format\n", disk->name);
        return;
    }

    for (size_t i = 0; i < IDE_PART_NR; ++i) {
        entry = &boot->entry[i];
        part = &disk->parts[i];
        if (!entry->count) {
            continue;
        }

        // 设置分区信息到 disk->part[i]
        sprintf(part->name, "%s%d", disk->name, i + 1);
        part->disk = disk;
        part->count = entry->count;
        part->system = entry->system;
        part->start = entry->start;

        LOGK("Disk Part Info: %s\n", part->name);
        LOGK(" >> bootable: %d\n", entry->bootable);
        LOGK(" >> start   : %d\n", entry->start);
        LOGK(" >> count   : %d\n", entry->count);
        LOGK(" >> system  : 0x%x\n", entry->system);

        // 扩展分区单独处理（暂不支持，需要扩大disk->part[N]，动态大小不是很方便）
        if (entry->system == PART_FS_EXTENDED) {
            LOGK("WARN: Disk Extened Partition Unsupport\n");

            eboot = (boot_sector_t*)(buf +
                                     SECTOR_SIZE);  // 使用buf后半，防止覆盖boot
            ide_pio_read(disk, (void*)eboot, 1, entry->start);

            for (size_t j = 0; j < IDE_PART_NR; ++j) {
                eentry = &eboot->entry[j];
                if (!eentry->count) {
                    continue;
                }
                LOGK("Disk Extend Part Info: %s part %d extend %d\n",
                       part->name, i, j);
                LOGK(" >> bootable: %d\n", eentry->bootable);
                LOGK(" >> start   : %d\n", eentry->start);
                LOGK(" >> count   : %d\n", eentry->count);
                LOGK(" >> system  : 0x%x\n", eentry->system);
            }
        }
    }
    LOGK("Disk %s Partition Information Checked\n", disk->name);
}

static void ide_fixstrings(char* buf, u32 len) {
    for (size_t i = 0; i < len; i += 2) {
        register char ch = buf[i];
        buf[i] = buf[i + 1];
        buf[i + 1] = ch;
    }
    buf[len - 1] = '\0';
}

static u32 ide_identify(ide_disk_t* disk, u16* buf) {
    LOGK("Identifing Disk %s\n", disk->name);
    // IDE设备不支持热插拔，因此初始化过程无需互斥锁
    // mutex_lock(&disk->ctrl->lock);

    ide_select_drive(disk);

    outb(disk->ctrl->iobase + IDE_COMMAND, IDE_CMD_IDENTIFY);

    ide_busy_wait(disk->ctrl, IDE_SR_NULL);

    ide_params_t* params = (ide_params_t*)buf;

    ide_pio_read_sector(disk, buf);

    u32 ret = EOF;
    if (params->total_lba == 0) {
        LOGK("Disk %s does'nt exsit\n", disk->name);
        goto rollback;
    }

    ide_fixstrings(params->serial, sizeof(params->serial));
    ide_fixstrings(params->firmware, sizeof(params->firmware));
    ide_fixstrings(params->serial, sizeof(params->serial));

    LOGK("Find Disk %s:\n", disk->name);

    LOGK("  > total lba        :%d\n", params->total_lba);
    LOGK("  > serial number    :%s\n", params->serial);
    LOGK("  > firmware version :%s\n", params->firmware);
    LOGK("  > modle numberl    :%s\n", params->serial);

    disk->total_lba = params->total_lba;
    disk->cylinders = params->cylinders;
    disk->heads = params->heads;
    disk->sectors = params->sectors;
    ret = 0;
rollback:
    // mutex_unlock(&disk->ctrl->lock);
    return ret;
}

static void ide_ctrl_init() {
    u16* buf = (u16*)alloc_kpage(1);
    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ide_ctrl_t* ctrl = &controllers[cidx];
        sprintf(ctrl->name, "ide%u", cidx);
        mutex_init(&ctrl->lock);

        ctrl->active = NULL;
        ctrl->waiter = NULL;
        ctrl->iobase =
            cidx ? IDE_IOBASE_SECONDARY : IDE_IOBASE_PRIMARY;  // 主从通道
        ctrl->control = inb(ctrl->iobase + IDE_CONTROL);

        for (size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            ide_disk_t* disk = &ctrl->disks[didx];
            sprintf(disk->name, "hd%c",
                    'a' + cidx * 2 + didx);  // hd[abcd] 四个盘
            disk->ctrl = ctrl;
            disk->sector_size = SECTOR_SIZE;
            disk->selector = didx ? IDE_LBA_SLAVE : IDE_LBA_MASTER;  // 主从盘
            disk->master = (didx == 0);
            if (ide_identify(disk, buf) == 0) {
                ide_part_init(disk, buf);
            }
            // break;
        }
        // break;
    }
    free_kpage((u32)buf, 1);
}

// 根据分区，安装IDE设备
static void ide_install() {
    ide_ctrl_t* ctrl;
    ide_disk_t* disk;
    ide_part_t* part;
    dev_t dev_parent;
    for (size_t cidx = 0; cidx < IDE_CTRL_NR; cidx++) {
        ctrl = &controllers[cidx];
        for (size_t didx = 0; didx < IDE_DISK_NR; didx++) {
            // 注册IDE硬盘（hda-hdd）
            disk = &ctrl->disks[didx];
            if (!disk->total_lba)
                continue;
            dev_parent = device_install(DEV_BLOCK, DEV_IDE_DISK, disk, disk->name, 0,
                                 ide_pio_ioctl, ide_pio_read, ide_pio_write);
            for (size_t i = 0; i < IDE_PART_NR; ++i) {
                // 注册每个盘中四个分区
                part = &disk->parts[i];
                if (!part->count)
                    continue;
                device_install(DEV_BLOCK, DEV_IDE_PART, part, part->name, dev_parent,
                               ide_pio_part_ioctl, ide_pio_part_read,
                               ide_pio_part_write);
            }
        }
    }
}

// ide 硬盘初始化
void ide_init() {
    ide_ctrl_init();

    // 注册硬盘中断，并打开屏蔽字
    set_interrupt_handler(IRQ_HARDDISK, ide_handler);
    set_interrupt_handler(IRQ_HARDDISK2, ide_handler);
    set_interrupt_mask(IRQ_HARDDISK, true);
    set_interrupt_mask(IRQ_HARDDISK2, true);
    set_interrupt_mask(IRQ_CASCADE, true);

    ide_install();  // 安装设备

    LOGK("ide initialized...\n");
}

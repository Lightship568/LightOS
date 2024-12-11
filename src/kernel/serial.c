#include <lib/debug.h>
#include <lib/io.h>
#include <lib/kfifo.h>
#include <lib/mutex.h>
#include <lib/stdio.h>
#include <lightos/device.h>
#include <lightos/interrupt.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <lib/vsprintf.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)
// #define LOGK(fmt, args...) ;

#define COM1_IOBASE 0x3F8  // 串口 1 基地址
#define COM2_IOBASE 0x2F8  // 串口 2 基地址

#define COM_DATA 0           // 数据寄存器
#define COM_INTR_ENABLE 1    // 中断允许
#define COM_BAUD_LSB 0       // 波特率低字节
#define COM_BAUD_MSB 1       // 波特率高字节
#define COM_INTR_IDENTIFY 2  // 中断识别
#define COM_LINE_CONTROL 3   // 线控制
#define COM_MODEM_CONTROL 4  // 调制解调器控制
#define COM_LINE_STATUS 5    // 线状态
#define COM_MODEM_STATUS 6   // 调制解调器状态

// 线状态
#define LSR_DR 0x1
#define LSR_OE 0x2
#define LSR_PE 0x4
#define LSR_FE 0x8
#define LSR_BI 0x10
#define LSR_THRE 0x20
#define LSR_TEMT 0x40
#define LSR_IE 0x80

#define BUF_LEN 64

typedef struct serial_t {
    u16 iobase;            // 端口号基地址
    kfifo_t rx_fifo;       // 读 fifo
    char rx_buf[BUF_LEN];  // 读 缓冲
    mutex_t rlock;         // 读锁
    task_t* rx_waiter;     // 读等待任务
    mutex_t wlock;         // 写锁
    task_t* tx_waiter;     // 写等待任务
} serial_t;

static serial_t serials[2];

void recv_data(serial_t* serial) {
    char ch = inb(serial->iobase);
    if (ch == '\r') {  // 特殊处理，回车键直接换行
        ch = '\n';
    }
    kfifo_put(&serial->rx_fifo, ch);
    if (serial->rx_waiter != NULL) {
        task_intr_unblock_no_waiting_list(serial->rx_waiter);
        serial->rx_waiter = NULL;
    }
}

// 中断处理函数
void serial_handler(int vector) {
    u32 irq = vector - 0x20;
    assert(irq == IRQ_SERIAL_1 || irq == IRQ_SERIAL_2);
    send_eoi(vector);

    serial_t* serial = &serials[irq - IRQ_SERIAL_1];
    u8 state = inb(serial->iobase + COM_LINE_STATUS);

    if (state & LSR_DR) {  // 数据可读
        recv_data(serial);
    }

    // 如果可以发送数据，并且写进程阻塞
    if ((state & LSR_THRE) && serial->tx_waiter) {
        task_intr_unblock_no_waiting_list(serial->tx_waiter);
        serial->tx_waiter = NULL;
    }
}

int serial_read(serial_t* serial, char* buf, u32 count) {
    mutex_lock(&serial->rlock);
    int nr = 0;
    while (nr < count) {
        while (kfifo_empty(&serial->rx_fifo)) {
            assert(serial->rx_waiter == NULL);
            serial->rx_waiter = get_current();
            task_block(serial->rx_waiter, NULL, TASK_BLOCKED);
        }
        buf[nr++] = kfifo_get(&serial->rx_fifo);
    }
    mutex_unlock(&serial->rlock);
    return nr;
}

int serial_write(serial_t* serial, char* buf, u32 count) {
    mutex_lock(&serial->wlock);
    int nr = 0;
    while (nr < count) {
        u8 state = inb(serial->iobase + COM_LINE_STATUS);
        if (state & LSR_THRE) { // 如果串口可写
            outb(serial->iobase, buf[nr++]);
            continue;
        }
        // task_t *task = get_current();
        // serial->tx_waiter = task;
        // task_block(task, NULL, TASK_BLOCKED, TIMELESS);
    }
    mutex_unlock(&serial->wlock);
    return nr;
}

// 初始化串口
void serial_init(void) {
    for (size_t i = 0; i < 2; i++) {
        serial_t* serial = &serials[i];
        kfifo_init(&serial->rx_fifo, serial->rx_buf, BUF_LEN);
        serial->rx_waiter = NULL;
        mutex_init(&serial->rlock);
        serial->tx_waiter = NULL;
        mutex_init(&serial->wlock);

        u16 irq;
        if (!i) {
            irq = IRQ_SERIAL_1;
            serial->iobase = COM1_IOBASE;
        } else {
            irq = IRQ_SERIAL_2;
            serial->iobase = COM2_IOBASE;
        }

        // 禁用中断
        outb(serial->iobase + COM_INTR_ENABLE, 0);

        // 激活 DLAB
        outb(serial->iobase + COM_LINE_CONTROL, 0x80);

        // 设置波特率因子 0x0030
        // 波特率为 115200 / 0x30 = 115200 / 48 = 2400
        outb(serial->iobase + COM_BAUD_LSB, 0x30);
        outb(serial->iobase + COM_BAUD_MSB, 0x00);

        // 复位 DLAB 位，数据位为 8 位
        outb(serial->iobase + COM_LINE_CONTROL, 0x03);

        // 0x0d = 0b1101
        // 0x0f 无法读串口输入
        // 数据可用 + 中断/错误 + 状态变化 都发送中断
        outb(serial->iobase + COM_INTR_ENABLE, 0x0d); 

        // 启用 FIFO, 清空 FIFO, 14 字节触发电平
        outb(serial->iobase + COM_INTR_IDENTIFY, 0xC7);

        // 设置回环模式，测试串口芯片
        outb(serial->iobase + COM_MODEM_CONTROL, 0b11011);

        // 发送字节
        outb(serial->iobase, 0xAE);

        // 收到的内容与发送的不一致，则串口不可用
        if (inb(serial->iobase) != 0xAE) {
            continue;
        }

        // 设置回原来的模式
        outb(serial->iobase + COM_MODEM_CONTROL, 0b1011);

        // 注册中断函数
        set_interrupt_handler(irq, serial_handler);

        // 打开中断屏蔽
        set_interrupt_mask(irq, true);

        char name[16];

        sprintf(name, "com%d", i + 1);

        device_install(DEV_CHAR, DEV_SERIAL, serial, name, 0, NULL, serial_read,
                       serial_write);

        LOGK("Serial 0x%x initialized as %s\n", serial->iobase, name);
    }
}
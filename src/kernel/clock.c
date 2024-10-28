/**
 * 时钟 
 * 100 HZ   
 * JIFFY 
*/

#include <lib/debug.h>
#include <lib/io.h>
#include <lightos/interrupt.h>
#include <lightos/task.h>
#include <sys/assert.h>
#include <lib/io.h>
#include <lib/stdlib.h>

#define PIT_CHAN0_REG 0X40
#define PIT_CHAN2_REG 0X42
#define PIT_CTRL_REG 0X43

#define HZ 100
#define OSCILLATOR 1193182
#define CLOCK_COUNTER (OSCILLATOR / HZ)
#define JIFFY (1000 / HZ)

#define SPEAKER_REG 0x61 //历史遗留问题，这个也是键盘端口
#define BEEP_HZ 440
#define BEEP_COUNTER (OSCILLATOR / BEEP_HZ)
#define BEEP_JIFFY 100

u32 volatile jiffies = 0;
u32 jiffy = JIFFY;
u32 volatile beeping = 0;

void start_beep(void) {
    outb(SPEAKER_REG, inb(SPEAKER_REG) | 0b11);
    beeping = jiffies + BEEP_JIFFY;
}
void stop_beep(void){
    if (beeping && jiffies > beeping){
        outb(SPEAKER_REG, inb(SPEAKER_REG) & 0xfc);
        beeping = 0;
    }
}

// 时钟中断，每隔 JIFFY （10ms）调用一次
void clock_handler(int vector) {
    assert(vector == 0x20);

    jiffies++;

    // timer_wakeup();
    
    // 唤醒睡眠进程
    task_wakeup();

    task_t* current = get_current();
    assert(current->magic == LIGHTOS_MAGIC);

    current->jiffies = jiffies;
    current->ticks--;

    // 发送中断处理结束（允许下一次中断到来，否则切进程就没中断了）
    send_eoi(vector); 

    if (current->ticks <= 0) {
        current->ticks = current->priority;
        schedule(); // 程序再次被调度时将会从这里继续执行，后续是iret。且为关中断状态，中断不会重入
    }
}


extern u32 startup_time;

time_t sys_time(void) {
    return startup_time + (jiffies * JIFFY) / 1000;
}

void pit_init(void) {
    // 配置计数器 0 时钟
    outb(PIT_CTRL_REG, 0b00110100);
    outb(PIT_CHAN0_REG, CLOCK_COUNTER & 0xff);
    outb(PIT_CHAN0_REG, (CLOCK_COUNTER >> 8) & 0xff);

    // 配置计数器 2 蜂鸣器
    outb(PIT_CTRL_REG, 0b10110110);
    outb(PIT_CHAN2_REG, (u8)BEEP_COUNTER);
    outb(PIT_CHAN2_REG, (u8)(BEEP_COUNTER >> 8));
}

// 时钟初始化，在 interrupt_init 后调用
// 时钟中断每隔 JIFFY 触发一次，交由 clock_handler 处理
void clock_init(void) {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
    DEBUGK("Clock Initialized\n");
}

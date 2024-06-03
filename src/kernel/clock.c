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

void clock_handler(int vector) {
    assert(vector == 0x20);
    send_eoi(vector);  // 发送中断处理结束

    jiffies++;

    if (jiffies % 200 == 0){
        DEBUGK("clock jiffies %d ...\n", jiffies);

        start_beep();
    }
    stop_beep();

    // timer_wakeup();

    // task_t* task = running_task();
    // assert(task->magic == ONIX_MAGIC);

    // task->jiffies = jiffies;
    // task->ticks--;
    // if (!task->ticks) {
    //     schedule();
    // }
}

extern u32 startup_time;

// time_t sys_time(void) {
//     return startup_time + (jiffies * JIFFY) / 1000;
// }

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

void clock_init(void) {
    pit_init();
    set_interrupt_handler(IRQ_CLOCK, clock_handler);
    set_interrupt_mask(IRQ_CLOCK, true);
    DEBUGK("Clock Initialized\n");
}

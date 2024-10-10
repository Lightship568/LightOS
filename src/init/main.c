/**
 * main.c
 */

#include <lib/debug.h>
#include <lib/print.h>
#include <lib/stdlib.h>
#include <lightos/console.h>
#include <lightos/interrupt.h>
#include <lightos/lightos.h>
#include <lightos/memory.h>
#include <lightos/rtc.h>
#include <lightos/syscall.h>
#include <lightos/time.h>
#include <sys/assert.h>
#include <sys/global.h>
#include <sys/types.h>
#include <lightos/task.h>

extern void clock_init();  // clock无.h文件

char message[] =
    "hello LightOS!!!111111111111111111111111111111111111111111111\n\0";

/**
 * 测试用
 */
extern void schedule();

void kernel_init(void) {
    time_init();
    gdt_init();
    interrupt_init();
    memory_map_init();

    clock_init();
    rtc_init();

    mapping_init();

// 页表映射测试

    // char* ptr = (char *)0x7fffff;
    // printk("0x%p: %s\n", ptr,ptr);

    // 不开中断，让进程初始化之后在内部开中断，防止clock_handler找不到进程panic
    // start_interrupt();

// 测试 syscall 和 task

    syscall_init();
    task_init();
    task_test();

    // DEBUGK("return value is %d\n", ret);

    // move_to_user_mode();

    // for (;;){
    //     schedule();
    // }
    hang(); 
}
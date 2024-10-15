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
extern void schedule(void);
extern void tss_init(void);
extern void idle(void);

void kernel_init(void) {
    tss_init(); // 初始化 GDT[3] TSS
    time_init();
    interrupt_init();
    memory_map_init();

    clock_init();
    rtc_init();

    mapping_init();

// 页表映射测试

    // char* ptr = (char *)0x7fffff;
    // printk("0x%p: %s\n", ptr,ptr);

// 测试 syscall 和 task

    syscall_init();
    task_init();

    // DEBUGK("return value is %d\n", ret);

    idle(); //ba
}
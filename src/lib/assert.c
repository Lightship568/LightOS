#include <lib/stdio.h>
#include <lib/vsprintf.h>
#include <sys/assert.h>
#include <sys/stdarg.h>

/**
 * 用户态 libc 使用的 assert
 */

static char buf[1024];

// 强制阻塞
static void spin(char* name) {
    printf("spinning in %s ...\n", name);
    for (;;)
        ;
}

void assertion_failure(char* exp, char* file, char* base, int line) {
    printf(
        "[assert] -->assert failed!!!\n"
        "--> assert(%s) \n"
        "--> file: %s \n"
        "--> base: 0x%x \n"
        "--> line: %d \n",
        exp, file, base, line);

    spin("assertion_failure()");

    // 不可能走到这里，否则出错；
    asm volatile("ud2");
}

void panic(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);

    printf("[panic] --> %s \n", buf);
    spin("panic()");

    // 不可能走到这里，否则出错；
    asm volatile("ud2");
}
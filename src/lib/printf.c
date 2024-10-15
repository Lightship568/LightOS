#include <lib/stdio.h>
#include <sys/stdarg.h>
#include <sys/types.h>
#include <lib/vsprintf.h>
#include <lib/syscall.h>

// 用户态 printf

static char buf[1024];

int printf(const char *fmt, ...)
{
    va_list args;
    int i;

    va_start(args, fmt);

    i = vsprintf(buf, fmt, args);

    va_end(args);

    write(stdout, buf, i);

    return i;
}
#include <lib/debug.h>
#include <sys/stdarg.h>
#include <lib/print.h>
#include <lib/vsprintf.h>

static char buf[1024];

void debugk(char *file, int line, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    printk("[DEBUG] [line %d in %s]: %s", line, file, buf);
}
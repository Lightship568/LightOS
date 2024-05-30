#include <lib/print.h>
#include <lib/stdarg.h>
#include <lightos/console.h>
#include <lib/vsprintf.h>

static char buf[1024];
int printk(const char* fmt, ...){
    int cnt;
    va_list args;
    va_start(args, fmt);
    cnt = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, cnt);
    return cnt;
}
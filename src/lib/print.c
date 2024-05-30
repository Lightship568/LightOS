#include <lib/print.h>
#include <sys/stdarg.h>
#include <lightos/console.h>
#include <lib/vsprintf.h>
#include <lib/string.h>

static char buf[1024];
int printk(const char* fmt, ...){
    int cnt;
    va_list args;
    va_start(args, fmt);
    memset(buf, 0, sizeof(buf));
    cnt = vsprintf(buf, fmt, args);
    va_end(args);
    console_write(buf, cnt);
    return cnt;
}
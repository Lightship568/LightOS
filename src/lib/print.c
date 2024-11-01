#include <lib/print.h>
#include <sys/stdarg.h>
#include <lightos/console.h>
#include <lib/vsprintf.h>
#include <lib/string.h>
#include <lightos/device.h>
#include <sys/assert.h>

extern int32 console_write(void* dev, char *buf, u32 count);
static char buf[1024];
int printk(const char* fmt, ...){
    int cnt;
    va_list args;
    va_start(args, fmt);
    memset(buf, 0, sizeof(buf));
    cnt = vsprintf(buf, fmt, args);
    va_end(args);
    // device_init 要早于最初的设备注册（console_init）
    device_t* device = device_find(DEV_CONSOLE, 0);
    assert(device);
    device_write(device->dev, buf, cnt, 0, 0);
    // console_write(NULL, buf, cnt);
    return cnt;
}
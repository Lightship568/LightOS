#include <lib/debug.h>
#include <lib/print.h>
#include <lib/vsprintf.h>
#include <lightos/device.h>
#include <sys/assert.h>
#include <sys/stdarg.h>

static char buf[1024];

static device_t* debug_output_device = NULL;

void debugk(char* file, int line, const char* fmt, ...) {
    if (!debug_output_device) {
        debug_output_device = device_find(DEV_SERIAL, 0);
        assert(debug_output_device);
    }
    device_t* device = debug_output_device;
    int cnt = sprintf(buf, "[DEBUG] [%s:%d]: ", file, line);
    device_write(device->dev, buf, cnt, 0, 0);

    va_list args;
    va_start(args, fmt);
    cnt = vsprintf(buf, fmt, args);
    va_end(args);
    device_write(device->dev, buf, cnt, 0, 0);
}
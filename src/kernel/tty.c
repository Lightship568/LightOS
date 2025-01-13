#include <lib/debug.h>
#include <lightos/device.h>
#include <lightos/tty.h>
#include <sys/errno.h>

#define LOGK(fmt, args...) DEBUGK(fmt, ##args)

static tty_t typewriter;

int tty_rx_notify(char* ch, bool ctrl, bool shift, bool alt) {
    switch (*ch) {
        case '\r':
            *ch = '\n';
            break;
        default:
            break;
    }
    if (!ctrl)
        return 0;

    // 处理 控制输入
    tty_t* tty = &typewriter;
    switch (*ch) {
        case 'c':
        case 'C':
            LOGK("CTRL + C Pressed\n");
            // tty_intr();
            *ch = '\n';
            return 0;
        case 'l':
        case 'L':
            // 清屏
            device_write(tty->wdev, "\x1b[2J\x1b[0;0H\r", 12, 0, 0);
            *ch = '\r';
            return 0;
        default:
            return 1;
    }
    return 1;
}

int tty_read(tty_t* tty, char* buf, u32 count) {
    return device_read(tty->rdev, buf, count, 0, 0);
}

int tty_write(tty_t* tty, char* buf, u32 count) {
    return device_write(tty->wdev, buf, count, 0, 0);
}

int tty_ioctl(tty_t* tty, int cmd, void* args, int flags) {
    switch (cmd) {
        case TIOCSPGRP:
            tty->pgid = (pid_t)args;
            return EOK;
        default:
            break;
    }
    return -EINVAL;
}

int sys_stty(void) {
    return -ENOSYS;
}

int sys_gtty(void) {
    return -ENOSYS;
}

void tty_init(void) {
    device_t* device = NULL;

    tty_t* tty = &typewriter;

    // 输入设备是键盘
    device = device_find(DEV_KEYBOARD, 0);
    tty->rdev = device->dev;

    // 输出设备是控制台
    device = device_find(DEV_CONSOLE, 0);
    tty->echo = true;
    tty->wdev = device->dev;

    device_install(DEV_CHAR, DEV_TTY, tty, "tty", 0, tty_ioctl, tty_read,
                   tty_write);
}
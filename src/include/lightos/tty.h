#ifndef LIGHTOS_TTY_H

#include <sys/types.h>

typedef struct tty_t {
    dev_t rdev;  // 读设备号
    dev_t wdev;  // 写设备号
    pid_t pgid;  // 写进程组
    bool echo;   // 回显标志
} tty_t;

// ioctl 设置进程组命令
#define TIOCSPGRP 0x5410

void tty_init(void);

#endif
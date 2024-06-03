#include <stdio.h>
#include <unistd.h>
#include <sys/io.h>

#define SPEAKER_REG 0x61

void beep() {
    // 允许程序访问端口
    if (ioperm(SPEAKER_REG, 1, 1)) {
        perror("ioperm");
        return;
    }

    // 启动蜂鸣器
    outb(inb(SPEAKER_REG) | 0b11, SPEAKER_REG);
    usleep(500000);  // 延迟 500 毫秒

    // 停止蜂鸣器
    outb(inb(SPEAKER_REG) & 0xfc, SPEAKER_REG);

    // 禁止程序访问端口
    if (ioperm(SPEAKER_REG, 1, 0)) {
        perror("ioperm");
        return;
    }
}

int main() {
    beep();
    return 0;
}

#ifndef LIGHTOS_STDLIB_H
#define LIGHTOS_STDLIB_H

#include <sys/types.h>

#define MAX(a, b) (a < b ? b : a)
#define MIN(a, b) (a < b ? a : b)

// 当前实现 delay 是通过 set_alarm 置 flag 从而退出 while 循环
void delay(u32 second);
void hang(void);

u8 bcd_to_bin(u8 value);
u8 bin_to_bcd(u8 value);

u32 div_round_up(u32 num, u32 size);

#endif
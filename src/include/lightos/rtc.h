#ifndef LIGHTOS_RTC_H
#define LIGHTOS_RTC_H

/**
 * RTC 相关
 * 
*/

#include <sys/types.h>

void set_alarm(u32 secs, bool* flag);
u8 cmos_read(u8 addr);
void cmos_write(u8 addr, u8 value);
void rtc_init(void);

#endif
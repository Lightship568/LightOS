#include <lib/debug.h>
#include <lib/stdlib.h>
#include <lightos/rtc.h>
#include <lightos/time.h>

#define CMOS_ADDR 0x70  // CMOS 地址寄存器
#define CMOS_DATA 0x71  // CMOS 数据寄存器

// 下面是 CMOS 信息的寄存器索引
#define CMOS_SECOND 0x00   // (0 ~ 59)
#define CMOS_MINUTE 0x02   // (0 ~ 59)
#define CMOS_HOUR 0x04     // (0 ~ 23)
#define CMOS_WEEKDAY 0x06  // (1 ~ 7) 星期天 = 1，星期六 = 7
#define CMOS_DAY 0x07      // (1 ~ 31)
#define CMOS_MONTH 0x08    // (1 ~ 12)
#define CMOS_YEAR 0x09     // (0 ~ 99)
#define CMOS_CENTURY 0x32  // 可能不存在
#define CMOS_NMI 0x80

// 系统启动时间戳，自1970
time_t startup_time;
// 世纪值
int century;

void time_read_bcd(tm* time) {
    // CMOS 的访问速度很慢。为了减小时间误差，在读取了下面循环中所有数值后，
    // 若此时 CMOS 中秒值发生了变化，那么就重新读取所有值。
    // 这样内核就能把与 CMOS 的时间误差控制在 1 秒之内。
    do {
        time->tm_sec = cmos_read(CMOS_SECOND);
        time->tm_min = cmos_read(CMOS_MINUTE);
        time->tm_hour = cmos_read(CMOS_HOUR);
        time->tm_wday = cmos_read(CMOS_WEEKDAY);
        time->tm_mday = cmos_read(CMOS_DAY);
        time->tm_mon = cmos_read(CMOS_MONTH);
        time->tm_year = cmos_read(CMOS_YEAR);
        century = cmos_read(CMOS_CENTURY);
    } while (time->tm_sec != cmos_read(CMOS_SECOND));
}

void time_read(tm* time) {
    time_read_bcd(time);
    time->tm_sec = bcd_to_bin(time->tm_sec);
    time->tm_min = bcd_to_bin(time->tm_min);
    time->tm_hour = bcd_to_bin(time->tm_hour);
    time->tm_wday = bcd_to_bin(time->tm_wday);
    time->tm_mday = bcd_to_bin(time->tm_mday);
    time->tm_mon = bcd_to_bin(time->tm_mon);
    time->tm_year = bcd_to_bin(time->tm_year);
    time->tm_yday = get_yday(time);
    time->tm_isdst = -1;
    century = bcd_to_bin(century);
}

void time_init(void) {
    tm time;
    time_read(&time);
    startup_time = mktime(&time);
    DEBUGK("startup time: %d%d-%02d-%02d %02d:%02d:%02d\n", century, time.tm_year,
         time.tm_mon, time.tm_mday, time.tm_hour, time.tm_min, time.tm_sec);
}
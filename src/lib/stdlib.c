#include <lib/stdlib.h>
#include <lightos/time.h>
#include <lightos/rtc.h>

void delay(u32 second){
    // while (second--);
    bool flag = true;
    set_alarm(second, &flag);
    while(flag);    
}

void count(void){
    while (true);
}

u8 bcd_to_bin(u8 value){
    return (value & 0xf) + (value >> 4) * 10;
}

u8 bin_to_bcd(u8 value){
    return ((value / 10) << 4) + (value % 10);
}
#include <lib/stdlib.h>
#include <lightos/time.h>
#include <lightos/rtc.h>
#include <lib/debug.h>

void delay(u32 second){
    bool flag = true;
    set_alarm(second, &flag);
    while(flag);    
}

void hang(void){
    DEBUGK("Hang...");
    while (true);
}

u8 bcd_to_bin(u8 value){
    return (value & 0xf) + (value >> 4) * 10;
}

u8 bin_to_bcd(u8 value){
    return ((value / 10) << 4) + (value % 10);
}

u32 div_round_up(u32 num, u32 size){
    return (num + size - 1) / size;
}
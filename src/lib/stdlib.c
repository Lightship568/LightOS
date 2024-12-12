#include <lib/stdlib.h>

u8 bcd_to_bin(u8 value) {
    return (value & 0xf) + (value >> 4) * 10;
}

u8 bin_to_bcd(u8 value) {
    return ((value / 10) << 4) + (value % 10);
}

u32 div_round_up(u32 num, u32 size) {
    return (num + size - 1) / size;
}

u32 atoi(const char* str) {
    u32 result = 0;
    int i = 0;

    if (str == NULL){
        return 0;
    }

    while (str[i] == ' ') {
        i++;
    }

    int sign = 1;  // 默认为正数
    if (str[i] == '-') {
        sign = -1;
        i++;
    } else if (str[i] == '+') {
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9') {
        result = result * 10 + (str[i] - '0');
        i++;
    }

    return result * sign;
}

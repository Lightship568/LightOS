#ifndef LIGHTOS_IO_H
#define LIGHTOS_IO_H

/**
 * io端口通信
 * 读写外部设备的内部寄存器（端口）
*/


#include <lightos/types.h>

extern u8 inb(u16 port);  // 输入一个字节
extern u16 inw(u16 port); // 输入一个字
extern u32 inl(u16 port); // 输入一个双字

extern void outb(u16 port, u8 value);  // 输出一个字节
extern void outw(u16 port, u16 value); // 输出一个字
extern void outl(u16 port, u32 value); // 输出一个双字
#endif

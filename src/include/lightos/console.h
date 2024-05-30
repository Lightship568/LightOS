#ifndef LIGHTOS_CONSOLE_H
#define LIGHTOS_CONSOLE_H

/**
 * 基础显卡驱动
 * CGA
*/

#include <lightos/types.h>

void console_init();
void console_clear();
void console_write(char *buf, u32 count);



#endif
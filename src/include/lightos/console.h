#ifndef LIGHTOS_CONSOLE_H
#define LIGHTOS_CONSOLE_H

/**
 * 基础显卡驱动
 * CGA
*/

#include <sys/types.h>

void console_init(void);
void console_clear(void);
void console_write(char *buf, u32 count);



#endif
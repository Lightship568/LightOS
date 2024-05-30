#ifndef LIGHTOS_DEBUG_H
#define LIGHTOS_DEBUG_H

void debugk(char *file, int line, const char *fmt, ...);

#define DEBUGK(fmt, args...) debugk(__BASE_FILE__, __LINE__, fmt, ##args)

#endif
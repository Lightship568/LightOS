#ifndef LIGHTOS_ASSERT_H
#define LIGHTOS_ASSERT_H

void assertion_failure(char *exp, char *file, char *base, int line);
void panic(const char *fmt, ...);

#define assert(exp)     \
    if (exp);           \
    else                \
        assertion_failure(#exp, __FILE__, __BASE_FILE__, __LINE__)

#endif
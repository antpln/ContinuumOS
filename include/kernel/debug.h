#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>
#include <stdarg.h>

void panic(const char* msg, const char* file, int line, const char* func, ...);
#define PANIC(msg, ...) panic(msg, __FILE__, __LINE__, __func__, ##__VA_ARGS__)
void debug(const char* fmt, ...);
void success(const char* fmt, ...);
void error(const char* fmt, ...);
void test(const char* fmt, ...);

#endif

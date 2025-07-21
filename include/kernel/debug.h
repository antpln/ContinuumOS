#ifndef DEBUG_H
#define DEBUG_H

#include <stdnoreturn.h>
#include <stdarg.h>

void panic(const char* fmt, ...);
void debug(const char* fmt, ...);
void success(const char* fmt, ...);
void error(const char* fmt, ...);
void test(const char* fmt, ...);

#endif

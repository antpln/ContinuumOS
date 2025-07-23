#ifndef _STDIO_H
#define _STDIO_H 1

#include <sys/cdefs.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define EOF (-1)

#ifdef __cplusplus
extern "C" {
#endif

int printf(const char* __restrict, ...);
int vprintf(const char* __restrict, va_list);
int putchar(int);
int puts(const char*);

// File I/O wrappers
int open(const char* path);
int read(int fd, uint8_t* buffer, size_t size);
int write(int fd, const uint8_t* buffer, size_t size);
void close(int fd);

// Keyboard input
int getchar();
int sprintf(char* str, const char* format, ...);

#ifdef __cplusplus
}
#endif

#endif
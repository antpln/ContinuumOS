#ifndef KERNEL_SYSCALLS_H
#define KERNEL_SYSCALLS_H
#include <stdint.h>
#include <stddef.h>

int sys_open(const char* path);
int sys_read(int fd, uint8_t* buffer, size_t size);
int sys_write(int fd, const uint8_t* buffer, size_t size);
void sys_close(int fd);
char sys_getchar();

#endif // KERNEL_SYSCALLS_H
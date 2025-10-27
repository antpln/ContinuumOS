#include <stdlib.h>
#include <sys/syscall.h>

void* malloc(size_t size)
{
    return syscall_alloc(size);
}

void free(void* ptr)
{
    syscall_free(ptr);
}

void* realloc(void* ptr, size_t size)
{
    return syscall_realloc(ptr, size);
}

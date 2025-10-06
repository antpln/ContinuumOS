#include <string.h>

// Add after your existing implementations

size_t strlen(const char *str)
{
    size_t len = 0;
    while (str[len] != '\0')
        len++;
    return len;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;
    
    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    
    // Pad with nulls if necessary
    for (; i < n; i++)
        dest[i] = '\0';
    
    return dest;
}


int memcmp(const void *aptr, const void *bptr, size_t size)
{
    const unsigned char *a = (const unsigned char *)aptr;
    const unsigned char *b = (const unsigned char *)bptr;
    for (size_t i = 0; i < size; i++)
    {
        if (a[i] < b[i])
            return -1;
        else if (b[i] < a[i])
            return 1;
    }
    return 0;
}
void *memcpy(void *dstptr, const void *srcptr, size_t size)
{
    auto dst = static_cast<unsigned char *>(dstptr);
    auto src = static_cast<const unsigned char *>(srcptr);
    for (size_t i = 0; i < size; i++)
        dst[i] = src[i];
    return dstptr;
}

char *strrchr(const char *str, int c)
{
    const char *last = NULL;
    while (*str != '\0')
    {
        if (*str == c)
        {
            last = str;
        }
        str++;
    }
    return (char *)last;
}
void *memmove(void *dstptr, const void *srcptr, size_t size)
{
    unsigned char *dst = (unsigned char *)dstptr;
    const unsigned char *src = (const unsigned char *)srcptr;
    if (dst < src)
    {
        for (size_t i = 0; i < size; i++)
            dst[i] = src[i];
    }
    else
    {
        for (size_t i = size; i != 0; i--)
            dst[i - 1] = src[i - 1];
    }
    return dstptr;
}
void *memset(void *bufptr, int value, size_t size)
{
    unsigned char *buf = (unsigned char *)bufptr;
    for (size_t i = 0; i < size; i++)
        buf[i] = (unsigned char)value;
    return bufptr;
}
char *strchr(const char *str, int c)
{
    while (*str != '\0')
    {
        if (*str == c)
        {
            return (char *)str;
        }
        str++;
    }
    return NULL;
}
int strcmp(const char *str1, const char *str2)
{
    while (*str1 && *str1 == *str2)
    {
        str1++;
        str2++;
    }
    return *(unsigned char *)str1 - *(unsigned char *)str2;
}

int strncmp(const char *str1, const char *str2, size_t n)
{
    while (n && *str1 && *str1 == *str2)
    {
        n--;
        str1++;
        str2++;
    }
    if (n == 0)
    {
        return 0;
    }
    return *(unsigned char *)str1 - *(unsigned char *)str2;
}

char *strcpy(char *dst, const char *src)
{
    char *ret = dst;
    while ((*dst++ = *src++) != '\0')
        ;
    return ret;
}

char *strcat(char *dst, const char *src)
{
    char *ret = dst;
    // Find end of destination string
    while (*dst != '\0')
        dst++;
    // Append source string
    while ((*dst++ = *src++) != '\0')
        ;
    return ret;
}

char *strtok(char *str, const char *delim)
{
    static char *last = NULL;
    if (str != NULL)
    {
        last = str;
    }
    else
    {
        if (last == NULL)
        {
            return NULL;
        }
    }

    // Skip leading delimiters.
    while (*last != '\0' && strchr(delim, *last) != NULL)
    {
        last++;
    }

    if (*last == '\0')
    {
        return NULL;
    }

    char *start = last;
    // Find the end of the token.
    while (*last != '\0' && strchr(delim, *last) == NULL)
    {
        last++;
    }

    if (*last != '\0')
    {
        *last = '\0';
        last++;
    }

    return start;
}
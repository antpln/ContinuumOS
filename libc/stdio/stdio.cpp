#include <stdio.h>
#include <kernel/syscalls.h>
#ifdef USER_APP_BUILD
#include <sys/syscall.h>
#endif
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

static void console_write(const char* data, size_t size)
{
    if (!data || size == 0)
    {
        return;
    }
#ifdef USER_APP_BUILD
    syscall_console_write(data, size);
#else
    sys_console_write(data, size);
#endif
}

static void console_write_string(const char* str)
{
    if (!str)
    {
        static const char null_str[] = "(null)";
        console_write(null_str, sizeof(null_str) - 1);
        return;
    }
    console_write(str, strlen(str));
}

int putchar(char c)
{
    console_write(&c, 1);
    return (unsigned char)c;
}

int puts(const char* str)
{
    console_write_string(str);
    const char newline = '\n';
    console_write(&newline, 1);
    return 0;
}

static void console_write_range(const char* data, size_t length)
{
    if (!data || length == 0)
    {
        return;
    }
    console_write(data, length);
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static size_t console_write_repeat(char ch, int count)
{
    if (count <= 0)
    {
        return 0;
    }

    char block[32];
    memset(block, ch, sizeof(block));

    int remaining = count;
    while (remaining > 0)
    {
        int chunk = remaining < (int)sizeof(block) ? remaining : (int)sizeof(block);
        console_write(block, (size_t)chunk);
        remaining -= chunk;
    }
    return (size_t)count;
}

static size_t format_unsigned(uint64_t value, unsigned base, char* buffer, bool uppercase)
{
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char temp[65];
    size_t pos = 0;

    do
    {
        temp[pos++] = digits[value % base];
        value /= base;
    } while (value != 0);

    for (size_t i = 0; i < pos; ++i)
    {
        buffer[i] = temp[pos - 1 - i];
    }
    return pos;
}

static int vprintf_internal(const char* format, va_list args)
{
    char literal_buffer[128];
    size_t literal_len = 0;
    char value_buffer[65];

    auto flush_literal = [&]() {
        if (literal_len > 0)
        {
            console_write(literal_buffer, literal_len);
            literal_len = 0;
        }
    };

    while (*format)
    {
        if (*format == '%')
        {
            ++format;
            flush_literal();

            bool left_adjust = false;
            bool force_sign = false;
            bool force_space = false;
            bool alternate = false;
            bool zero_pad = false;

            bool parsing_flags = true;
            while (parsing_flags)
            {
                switch (*format)
                {
                case '-':
                    left_adjust = true;
                    ++format;
                    break;
                case '+':
                    force_sign = true;
                    ++format;
                    break;
                case ' ':
                    force_space = true;
                    ++format;
                    break;
                case '#':
                    alternate = true;
                    ++format;
                    break;
                case '0':
                    zero_pad = true;
                    ++format;
                    break;
                default:
                    parsing_flags = false;
                    break;
                }
            }

            int width = 0;
            if (is_digit(*format))
            {
                while (is_digit(*format))
                {
                    width = width * 10 + (*format - '0');
                    ++format;
                }
            }

            int precision = -1;
            if (*format == '.')
            {
                ++format;
                precision = 0;
                while (is_digit(*format))
                {
                    precision = precision * 10 + (*format - '0');
                    ++format;
                }
                zero_pad = false; // Precision overrides zero padding
            }

            enum Length
            {
                LEN_NONE,
                LEN_LONG,
                LEN_LONGLONG
            };
            Length length = LEN_NONE;
            if (*format == 'l')
            {
                ++format;
                if (*format == 'l')
                {
                    ++format;
                    length = LEN_LONGLONG;
                }
                else
                {
                    length = LEN_LONG;
                }
            }

            char specifier = *format ? *format : '\0';
            if (!specifier)
            {
                break;
            }

            switch (specifier)
            {
            case 'd':
            case 'i':
            {
                int64_t value;
                if (length == LEN_LONGLONG)
                {
                    value = va_arg(args, long long);
                }
                else if (length == LEN_LONG)
                {
                    value = va_arg(args, long);
                }
                else
                {
                    value = va_arg(args, int);
                }

                bool negative = value < 0;
                uint64_t magnitude = negative ? static_cast<uint64_t>(-value) : static_cast<uint64_t>(value);

                size_t digits_len = (precision == 0 && magnitude == 0) ? 0 : format_unsigned(magnitude, 10, value_buffer, false);

                char prefix[2];
                size_t prefix_len = 0;
                if (negative)
                {
                    prefix[prefix_len++] = '-';
                }
                else if (force_sign)
                {
                    prefix[prefix_len++] = '+';
                }
                else if (force_space)
                {
                    prefix[prefix_len++] = ' ';
                }

                size_t zeroes = 0;
                if (precision > 0 && (size_t)precision > digits_len)
                {
                    zeroes = static_cast<size_t>(precision) - digits_len;
                }
                else if (digits_len == 0 && precision == 0)
                {
                    zeroes = 0;
                }
                else if (!left_adjust && zero_pad && width > (int)(digits_len + prefix_len))
                {
                    zeroes = static_cast<size_t>(width) - (digits_len + prefix_len);
                }

                size_t total_len = prefix_len + zeroes + digits_len;
                int padding = width > (int)total_len ? width - (int)total_len : 0;

                if (!left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                if (prefix_len)
                {
                    console_write_range(prefix, prefix_len);
                }
                console_write_repeat('0', (int)zeroes);
                if (digits_len)
                {
                    console_write_range(value_buffer, digits_len);
                }
                if (left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                break;
            }
            case 'u':
            {
                uint64_t value;
                if (length == LEN_LONGLONG)
                {
                    value = va_arg(args, unsigned long long);
                }
                else if (length == LEN_LONG)
                {
                    value = va_arg(args, unsigned long);
                }
                else
                {
                    value = va_arg(args, unsigned int);
                }

                size_t digits_len = (precision == 0 && value == 0) ? 0 : format_unsigned(value, 10, value_buffer, false);

                size_t zeroes = 0;
                if (precision > 0 && (size_t)precision > digits_len)
                {
                    zeroes = static_cast<size_t>(precision) - digits_len;
                }
                else if (!left_adjust && zero_pad && width > (int)digits_len)
                {
                    zeroes = static_cast<size_t>(width) - digits_len;
                }

                size_t total_len = zeroes + digits_len;
                int padding = width > (int)total_len ? width - (int)total_len : 0;

                if (!left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                console_write_repeat('0', (int)zeroes);
                if (digits_len)
                {
                    console_write_range(value_buffer, digits_len);
                }
                if (left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                break;
            }
            case 'x':
            case 'X':
            {
                bool uppercase = (specifier == 'X');
                uint64_t value;
                if (length == LEN_LONGLONG)
                {
                    value = va_arg(args, unsigned long long);
                }
                else if (length == LEN_LONG)
                {
                    value = va_arg(args, unsigned long);
                }
                else
                {
                    value = va_arg(args, unsigned int);
                }

                size_t digits_len = (precision == 0 && value == 0) ? 0 : format_unsigned(value, 16, value_buffer, uppercase);

                const char *prefix = nullptr;
                size_t prefix_len = 0;
                if (alternate && value != 0)
                {
                    prefix = uppercase ? "0X" : "0x";
                    prefix_len = 2;
                }

                size_t zeroes = 0;
                if (precision > 0 && (size_t)precision > digits_len)
                {
                    zeroes = static_cast<size_t>(precision) - digits_len;
                }
                else if (!left_adjust && zero_pad && width > (int)(digits_len + prefix_len))
                {
                    zeroes = static_cast<size_t>(width) - (digits_len + prefix_len);
                }

                size_t total_len = prefix_len + zeroes + digits_len;
                int padding = width > (int)total_len ? width - (int)total_len : 0;

                if (!left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                if (prefix_len)
                {
                    console_write_range(prefix, prefix_len);
                }
                console_write_repeat('0', (int)zeroes);
                if (digits_len)
                {
                    console_write_range(value_buffer, digits_len);
                }
                if (left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                break;
            }
            case 's':
            {
                const char* str = va_arg(args, const char*);
                const char* output = str ? str : "(null)";
                size_t len = strlen(output);
                if (precision >= 0 && (size_t)precision < len)
                {
                    len = (size_t)precision;
                }

                int padding = width > (int)len ? width - (int)len : 0;
                if (!left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                console_write_range(output, len);
                if (left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                break;
            }
            case 'c':
            {
                char c = (char)va_arg(args, int);
                int padding = width > 1 ? width - 1 : 0;
                if (!left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                console_write(&c, 1);
                if (left_adjust)
                {
                    console_write_repeat(' ', padding);
                }
                break;
            }
            case 'p':
            {
                void* ptr = va_arg(args, void*);
                uintptr_t raw = (uintptr_t)ptr;
                size_t digits_len = format_unsigned(raw, 16, value_buffer, false);
                const char prefix[] = "0x";
                size_t prefix_len = sizeof(prefix) - 1;

                if (precision > 0 && (size_t)precision > digits_len)
                {
                    size_t zeroes = (size_t)precision - digits_len;
                    size_t total_len = prefix_len + zeroes + digits_len;
                    int padding = width > (int)total_len ? width - (int)total_len : 0;
                    if (!left_adjust)
                    {
                        console_write_repeat(' ', padding);
                    }
                    console_write_range(prefix, prefix_len);
                    console_write_repeat('0', (int)zeroes);
                    console_write_range(value_buffer, digits_len);
                    if (left_adjust)
                    {
                        console_write_repeat(' ', padding);
                    }
                }
                else
                {
                    size_t total_len = prefix_len + digits_len;
                    int padding = width > (int)total_len ? width - (int)total_len : 0;
                    if (!left_adjust)
                    {
                        console_write_repeat(' ', padding);
                    }
                    console_write_range(prefix, prefix_len);
                    console_write_range(value_buffer, digits_len);
                    if (left_adjust)
                    {
                        console_write_repeat(' ', padding);
                    }
                }
                break;
            }
            case '%':
            {
                const char percent = '%';
                console_write(&percent, 1);
                break;
            }
            default:
            {
                const char percent = '%';
                console_write(&percent, 1);
                console_write(&specifier, 1);
                break;
            }
            }

            if (*format)
            {
                ++format;
            }
            continue;
        }
        else
        {
            if (literal_len >= sizeof(literal_buffer))
            {
                flush_literal();
            }
            literal_buffer[literal_len++] = *format;
        }
        ++format;
    }

    flush_literal();
    return 0;
}

// Convert integer to string (supports base 10 and 16)
int printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    int result = vprintf_internal(format, args);
    va_end(args);
    return result;
}

#ifdef __cplusplus
extern "C" {
#endif
int vprintf(const char* format, va_list args)
{
    return vprintf_internal(format, args);
}
#ifdef __cplusplus
}
#endif

// File I/O wrappers
int open(const char* path) {
    return sys_open(path);
}

int read(int fd, uint8_t* buffer, size_t size) {
    return sys_read(fd, buffer, size);
}

int write(int fd, const uint8_t* buffer, size_t size) {
    return sys_write(fd, buffer, size);
}

void close(int fd) {
    sys_close(fd);
}

// Keyboard input wrapper
int getchar() {
    return sys_getchar();
}

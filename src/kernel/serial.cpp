#include <kernel/serial.h>
#include <kernel/port_io.h>

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

#define COM1_PORT 0x3F8

static bool serial_initialized = false;

static inline uint8_t serial_read_status(void) {
    return inb(COM1_PORT + 5);
}

static inline void serial_wait_for_transmit_ready(void) {
    while ((serial_read_status() & 0x20) == 0) {
        // Spin until the transmitter holding register is empty
    }
}

void serial_init(void) {
    outb(COM1_PORT + 1, 0x00); // Disable interrupts
    outb(COM1_PORT + 3, 0x80); // Enable DLAB (set baud rate divisor)
    outb(COM1_PORT + 0, 0x01); // Divisor low byte (115200 baud)
    outb(COM1_PORT + 1, 0x00); // Divisor high byte
    outb(COM1_PORT + 3, 0x03); // 8 bits, no parity, one stop bit
    outb(COM1_PORT + 2, 0xC7); // Enable FIFO, clear them, 14-byte threshold
    outb(COM1_PORT + 4, 0x0B); // IRQs enabled, RTS/DSR set

    serial_initialized = true;
}

void serial_write_char(char c) {
    if (!serial_initialized) {
        serial_init();
    }

    if (c == '\n') {
        serial_write_char('\r');
    }

    serial_wait_for_transmit_ready();
    outb(COM1_PORT, (uint8_t)c);
}

void serial_write(const char *str) {
    if (!str) {
        return;
    }

    while (*str) {
        serial_write_char(*str++);
    }
}

static void serial_itoa(int32_t value, char *buffer, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char temp[32];
    int i = 0;
    int is_negative = 0;

    if (base == 10 && value < 0) {
        is_negative = 1;
        value = -value;
    }

    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    if (is_negative) {
        temp[i++] = '-';
    }

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

static void serial_utoa(uint32_t value, char *buffer, int base) {
    static const char digits[] = "0123456789ABCDEF";
    char temp[32];
    int i = 0;

    do {
        temp[i++] = digits[value % base];
        value /= base;
    } while (value > 0);

    int j = 0;
    while (i > 0) {
        buffer[j++] = temp[--i];
    }
    buffer[j] = '\0';
}

int serial_vprintf(const char *format, va_list args) {
    char buffer[32];

    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
            case 'd': {
                int num = va_arg(args, int);
                serial_itoa(num, buffer, 10);
                serial_write(buffer);
                break;
            }
            case 'u': {
                unsigned int num = va_arg(args, unsigned int);
                serial_utoa(num, buffer, 10);
                serial_write(buffer);
                break;
            }
            case 'x': {
                unsigned int num = va_arg(args, unsigned int);
                serial_utoa(num, buffer, 16);
                serial_write(buffer);
                break;
            }
            case 's': {
                const char *str = va_arg(args, const char *);
                if (str) {
                    serial_write(str);
                } else {
                    serial_write("(null)");
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                serial_write_char(c);
                break;
            }
            case 'p': {
                void *ptr = va_arg(args, void *);
                uintptr_t address = (uintptr_t)ptr;
                serial_write("0x");
                serial_utoa((uint32_t)address, buffer, 16);
                serial_write(buffer);
                break;
            }
            case '%': {
                serial_write_char('%');
                break;
            }
            default:
                serial_write_char('%');
                serial_write_char(*format);
                break;
            }
        } else {
            serial_write_char(*format);
        }
        format++;
    }

    return 0;
}

int serial_printf(const char *format, ...) {
    va_list args;
    va_start(args, format);
    int result = serial_vprintf(format, args);
    va_end(args);
    return result;
}

#include "utils.h"

size_t strlen(const char* str) {
    int len = 0;
    while (str[len]) {
        len++;
    }
    return len;
}
void int_to_string(int num, char* buffer) {
    int i = 0;
    int is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    if (num == 0) {
        buffer[i++] = '0';
    }
    while (num != 0) {
        buffer[i++] = num % 10 + '0';
        num /= 10;
    }
    if (is_negative) {
        buffer[i++] = '-';
    }
    buffer[i] = '\0';
    int len = strlen(buffer);
    for (int j = 0; j < len / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[len - j - 1];
        buffer[len - j - 1] = temp;
    }
}

// Convert unsigned int to ASCII, returns number of chars written
int uitoa(unsigned int value, char* out, int max_len) {
    char tmp[16];
    int ti = 0;
    if (value == 0) {
        if (max_len > 0) out[0] = '0';
        return 1;
    }
    while (value > 0 && ti < (int)sizeof(tmp)) {
        tmp[ti++] = '0' + (value % 10);
        value /= 10;
    }
    int wi = 0;
    while (ti > 0 && wi < max_len) {
        out[wi++] = tmp[--ti];
    }
    return wi;
}

uint16_t low_16(uint32_t addr) {
    return (uint16_t)(addr & 0xFFFF);
}
uint16_t high_16(uint32_t addr) {
    return (uint16_t)((addr >> 16) & 0xFFFF);
}
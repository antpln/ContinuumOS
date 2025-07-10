#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>
void int_to_string(int num, char* buffer);
int uitoa(unsigned int value, char* out, int max_len);
uint16_t low_16(uint32_t addr);
uint16_t high_16(uint32_t addr);

#endif
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <kernel/multiboot.h>

namespace framebuffer
{
struct FrameBufferInfo
{
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
    uint32_t bpp = 0;
    uintptr_t address = 0;
};

bool initialize(const multiboot_info_t *info);
bool is_available();
const FrameBufferInfo &info();

uint32_t framebuffer_physical_address();
uint32_t framebuffer_size();

uint32_t pack_color(uint8_t r, uint8_t g, uint8_t b);
void fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
void draw_mono_bitmap(uint32_t x,
                      uint32_t y,
                      uint32_t width,
                      uint32_t height,
                      const uint8_t *bitmap,
                      uint32_t stride,
                      uint32_t fg_color,
                      uint32_t bg_color,
                      bool transparent_bg);
uint32_t peek_pixel(uint32_t x, uint32_t y);
}

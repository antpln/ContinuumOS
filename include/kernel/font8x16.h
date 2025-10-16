#pragma once

#include <stdint.h>

namespace gui
{
constexpr uint32_t FONT_WIDTH = 8;
constexpr uint32_t FONT_HEIGHT = 16;

const uint8_t *glyph_for(char ch);
}

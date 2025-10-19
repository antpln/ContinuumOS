#pragma once

#include <stdint.h>

#define MOUSE_BUTTON_LEFT   0x01u
#define MOUSE_BUTTON_RIGHT  0x02u
#define MOUSE_BUTTON_MIDDLE 0x04u

typedef struct MouseEvent
{
    int32_t x;
    int32_t y;
    int16_t dx;
    int16_t dy;
    int8_t scroll_x;
    int8_t scroll_y;
    uint8_t buttons;
    uint8_t changed;
    int32_t target_pid;
} MouseEvent;

typedef struct MouseState
{
    int32_t x;
    int32_t y;
    uint8_t buttons;
    uint8_t available;
} MouseState;

#ifdef __cplusplus
extern "C" {
#endif

void mouse_initialize();
MouseState mouse_get_state();

#ifdef __cplusplus
}
#endif

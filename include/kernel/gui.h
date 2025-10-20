#pragma once

#include <stdint.h>
#include <sys/gui.h>
#include <kernel/mouse.h>

class Terminal;
struct Process;

namespace gui
{
void draw_boot_screen();
void draw_workspace(Terminal &terminal);
void process_command(const GuiCommand &command, Terminal &terminal, Process *requester);

uint32_t background_color_for_row(uint32_t y);
void fill_background_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void set_background_fill_override(uint32_t color);
void clear_background_fill_override();

void initialize_mouse_cursor(int32_t x, int32_t y, uint8_t buttons);
void handle_mouse_event(const MouseEvent &event, Terminal &terminal);
void begin_window_redraw();
void end_window_redraw();
}

#pragma once

#include <stdint.h>
#include <stddef.h>

class Terminal;
struct Process;

namespace terminal_windows
{
void init(Terminal &terminal, Process *initial_proc = nullptr);
void draw_windows(Terminal &terminal);
void request_new_window(Terminal &terminal, Process *proc);
void activate_process(Process *proc, Terminal &terminal);
void set_active_window_origin(Terminal &terminal, Process *proc, int32_t x, int32_t y);
void on_process_exit(Process *proc, Terminal &terminal);
void write_text(Terminal &terminal, Process *proc, const char *text, size_t length);
}

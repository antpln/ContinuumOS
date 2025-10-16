#ifndef LIBC_GUI_H
#define LIBC_GUI_H

#include <stdint.h>
#include <sys/gui.h>

#ifdef __cplusplus
extern "C" {
#endif

int gui_send_command(const GuiCommand *command);
void gui_request_redraw(void);
void gui_set_terminal_origin(int32_t x, int32_t y);
void gui_request_new_window(void);

#ifdef __cplusplus
}
#endif

#endif // LIBC_GUI_H

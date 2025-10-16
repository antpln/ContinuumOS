#include <gui.h>
#include <sys/syscall.h>

int gui_send_command(const GuiCommand *command)
{
    if (command == nullptr)
    {
        return -1;
    }
    syscall_gui_command(command);
    return 0;
}

void gui_request_redraw(void)
{
    GuiCommand cmd{};
    cmd.type = GUI_COMMAND_REDRAW;
    gui_send_command(&cmd);
}

void gui_set_terminal_origin(int32_t x, int32_t y)
{
    GuiCommand cmd{};
    cmd.type = GUI_COMMAND_SET_TERMINAL_ORIGIN;
    cmd.arg0 = x;
    cmd.arg1 = y;
    gui_send_command(&cmd);
}

void gui_request_new_window(void)
{
    GuiCommand cmd{};
    cmd.type = GUI_COMMAND_REQUEST_NEW_WINDOW;
    gui_send_command(&cmd);
}

#ifndef LIBC_SYS_GUI_H
#define LIBC_SYS_GUI_H



#include <stdint.h>



#ifdef __cplusplus
extern "C" {
#endif



typedef enum {
    GUI_COMMAND_REDRAW = 0,
    GUI_COMMAND_SET_TERMINAL_ORIGIN = 1,
    GUI_COMMAND_REQUEST_NEW_WINDOW = 2
} GuiCommandType;



typedef struct {
    uint32_t type;   // GuiCommandType
    int32_t arg0;
    int32_t arg1;
    uint32_t flags;
} GuiCommand;



#ifdef __cplusplus
}
#endif



#endif // LIBC_SYS_GUI_H

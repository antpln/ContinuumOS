#ifndef KERNEL_EDITOR_H
#define KERNEL_EDITOR_H

#include <kernel/keyboard.h>
#include <kernel/vfs.h>
#include <kernel/process.h>

#define EDITOR_MAX_LINES    128
#define EDITOR_LINE_LENGTH  128

class Editor {
public:
    Editor() = default;

    // Start editing `filename` in directory `current_dir`
    void start(const char* path);

    // True if we’re in editor mode
    bool is_active(void);

    // Feed one key event into the editor
    void handle_key(keyboard_event ke);

private:
    // Internal helpers
    void exit(bool save);
    void render();
    void draw_line(const char* text, int y, bool active_line);
    void put_cell(size_t x, size_t y, char ch, uint8_t color);
    void present_window();
    void update_cursor_visual(size_t row, size_t column, bool active);

    void handle_char(char c);
    void handle_enter();
    void handle_backspace();
    void handle_arrows(keyboard_event ke);

    // Status bar
    void draw_status_bar();
    void set_status_message(const char* msg);

    // ---- Editor state ----
    char buffer   [EDITOR_MAX_LINES][EDITOR_LINE_LENGTH]; // committed lines
    int  line_count;

    int  cursor_line;
    int  cursor_col;
    int  viewport_offset;

    char filename[64];
    char path[VFS_MAX_PATH];
    bool    active;

    char status_message[EDITOR_LINE_LENGTH]; // status bar message
    Process* owner_proc = nullptr;
};

void editor_start(const char* path);
bool editor_is_active(void);
void editor_handle_key(keyboard_event ke);

#endif // KERNEL_EDITOR_H

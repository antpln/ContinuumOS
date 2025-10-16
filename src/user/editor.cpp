#include <kernel/editor.h>
#include <stdio.h>
#include <string.h>
#include <kernel/heap.h>
#include <kernel/vga.h>
#include <kernel/vfs.h>
#include <kernel/scheduler.h>
#include <kernel/process.h>
#include <kernel/keyboard.h>
#include <process.h>
#include <utils.h>

extern Terminal terminal;

#define EDITOR_MAX_LINES        128
#define EDITOR_LINE_LENGTH      128
#define PREFIX_ACTIVE           "> "
#define PREFIX_ACTIVE_LEN       2
#define PREFIX_INACTIVE         "  "
#define PREFIX_INACTIVE_LEN     2

#define KEY_LEFT_ARROW          0x4B
#define KEY_RIGHT_ARROW         0x4D
#define KEY_UP_ARROW            0x48
#define KEY_DOWN_ARROW          0x50

static Editor editor_instance;

// Static copies of parameters set before process start
static char s_path[VFS_MAX_PATH];

extern "C" void editor_set_params(const char* path) {
    if (path) {
        strncpy(s_path, path, sizeof(s_path) - 1);
        s_path[sizeof(s_path) - 1] = '\0';
    } else {
        s_path[0] = '\0';
    }
}

// Copy a line with null-termination
static inline void copy_line(char* dst, const char* src) {
    strncpy(dst, src, EDITOR_LINE_LENGTH);
    dst[EDITOR_LINE_LENGTH - 1] = '\0';
}

// Start editing a file
void Editor::start(const char* path) {
    active = true;
    status_message[0] = '\0';

    if (path) {
        strncpy(this->path, path, sizeof(this->path) - 1);
        this->path[sizeof(this->path) - 1] = '\0';
    } else {
        this->path[0] = '\0';
    }

    const char* last_slash = strrchr(this->path, '/');
    if (last_slash && *(last_slash + 1)) {
        strncpy(this->filename, last_slash + 1, sizeof(this->filename) - 1);
    } else if (this->path[0]) {
        strncpy(this->filename, this->path, sizeof(this->filename) - 1);
    } else {
        strncpy(this->filename, "untitled", sizeof(this->filename) - 1);
    }
    this->filename[sizeof(this->filename) - 1] = '\0';

    line_count = 0;
    bool truncated = false;

    if (this->path[0]) {
        vfs_file_t file;
        if (vfs_open(this->path, &file) == VFS_SUCCESS) {
            char read_buf[128];
            char line_buf[EDITOR_LINE_LENGTH];
            int line_len = 0;
            int bytes = 0;

            while ((bytes = vfs_read(&file, read_buf, sizeof(read_buf))) > 0) {
                for (int i = 0; i < bytes; ++i) {
                    char c = read_buf[i];
                    if (c == '\r') {
                        continue;
                    }
                    if (c == '\n') {
                        line_buf[line_len] = '\0';
                        if (line_count < EDITOR_MAX_LINES) {
                            copy_line(buffer[line_count++], line_buf);
                        } else {
                            truncated = true;
                        }
                        line_len = 0;
                    } else {
                        if (line_len < EDITOR_LINE_LENGTH - 1) {
                            line_buf[line_len++] = c;
                        }
                    }
                }
            }

            if (line_len > 0 || line_count == 0) {
                line_buf[line_len] = '\0';
                if (line_count < EDITOR_MAX_LINES) {
                    copy_line(buffer[line_count++], line_buf);
                } else {
                    truncated = true;
                }
            }

            vfs_close(&file);
        }
    }

    if (line_count == 0) {
        buffer[0][0] = '\0';
        line_count = 1;
    }

    if (truncated) {
        set_status_message("File truncated in editor view");
    }

    cursor_line = 0;
    cursor_col = 0;
    viewport_offset = 0;

    render();
}

// Return true if editor is active
bool Editor::is_active() {
    return active;
}

// Exit the editor, optionally saving changes
void Editor::exit(bool save) {
    printf("\n");
    if (save) {
        if (!path[0]) {
            printf("Error: no path to save.\n");
            active = false;
            return;
        }

        while (line_count > 1 && strlen(buffer[line_count - 1]) == 0) {
            --line_count;
        }

        size_t total = 0;
        for (int i = 0; i < line_count; ++i) {
            total += strlen(buffer[i]) + 1; // include newline
        }

        char* data = nullptr;
        if (total > 0) {
            data = (char*)kmalloc(total);
        }
        if (total > 0 && !data) {
            printf("Memory allocation failed.\n");
            active = false;
            return;
        }

        size_t pos = 0;
        for (int i = 0; i < line_count; ++i) {
            size_t len = strlen(buffer[i]);
            if (data && len) {
                memcpy(data + pos, buffer[i], len);
            }
            pos += len;
            if (data) {
                data[pos] = '\n';
            }
            pos += 1;
        }

        int remove_res = vfs_remove(path);
        if (remove_res != VFS_SUCCESS && remove_res != VFS_NOT_FOUND) {
            printf("Error: could not prepare file '%s'.\n", path);
            if (data) kfree(data);
            active = false;
            return;
        }

        if (vfs_create(path) != VFS_SUCCESS) {
            // File may already exist; that's fine
        }

        vfs_file_t file;
        if (vfs_open(path, &file) != VFS_SUCCESS) {
            printf("Error: could not open file '%s'.\n", path);
            if (data) kfree(data);
            active = false;
            return;
        }

        if (vfs_seek(&file, 0) != VFS_SUCCESS) {
            printf("Error: could not seek file '%s'.\n", path);
            vfs_close(&file);
            if (data) kfree(data);
            active = false;
            return;
        }

        if (total > 0 && data) {
            int written = vfs_write(&file, data, total);
            if (written < 0 || (size_t)written != total) {
                printf("Error: failed to write file '%s'.\n", path);
                vfs_close(&file);
                if (data) kfree(data);
                active = false;
                return;
            }
        }

        vfs_close(&file);
        if (data) kfree(data);
        printf("File '%s' saved.\n", path);
    } else {
        printf("Edit aborted.\n");
    }
    active = false;
    // Clear the status-bar row
    int y = terminal.get_vga_height() - 1;
    size_t width = terminal.get_vga_width();
    for (size_t x = 0; x < width; ++x) {
        terminal.put_at(' ',
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        x, y);
    }
    // Do not kill the process or print the shell prompt here; leave that to editor_entry
}

// Draw a single line in the editor
void Editor::draw_line(const char* text, int y, bool is_active_line) {
    const char* prefix = is_active_line ? PREFIX_ACTIVE : PREFIX_INACTIVE;
    int prefix_len = is_active_line ? PREFIX_ACTIVE_LEN : PREFIX_INACTIVE_LEN;

    size_t width = terminal.get_vga_width();
    int max_content = (int)width - prefix_len;
    if (max_content < 0) max_content = 0;

    for (int i = 0; i < prefix_len && i < (int)width; ++i) {
        terminal.put_at(prefix[i],
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        i, y);
    }

    int len = strlen(text);
    if (len > max_content) len = max_content;
    for (int i = 0; i < len; ++i) {
        char c = text[i] ? text[i] : ' ';
        int x = prefix_len + i;
        auto color = (is_active_line && i == cursor_col)
                   ? terminal.make_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE)
                   : terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        if (x < (int)width)
            terminal.put_at(c, color, x, y);
    }

    for (int x = prefix_len + len; x < (int)width; ++x) {
        terminal.put_at(' ',
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        x, y);
    }
}

// Draw the status bar at the bottom
void Editor::draw_status_bar() {
    int y = terminal.get_vga_height() - 1;
    size_t width = terminal.get_vga_width();
    char line[EDITOR_LINE_LENGTH];
    int pos = 0;

    // "editing: filename"
    const char* lbl = "editing: ";
    for (int i = 0; lbl[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = lbl[i];
    for (int i = 0; filename[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = filename[i];

    // "  |  Ln X/Y"
    const char* mid1 = "  |  Ln ";
    for (int i = 0; mid1[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = mid1[i];
    pos += uitoa(cursor_line + 1, &line[pos], (int)sizeof(line) - pos);
    if (pos < (int)sizeof(line) - 1)
        line[pos++] = '/';
    pos += uitoa(line_count, &line[pos], (int)sizeof(line) - pos);

    // "  Col Z"
    const char* mid2 = "  Col ";
    for (int i = 0; mid2[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = mid2[i];
    pos += uitoa(cursor_col + 1, &line[pos], (int)sizeof(line) - pos);

    // "  |  MESSAGE"
    const char* mid3 = "  |  ";
    for (int i = 0; mid3[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = mid3[i];
    const char* msg = status_message[0] ? status_message : "EDITING";
    for (int i = 0; msg[i] && pos < (int)sizeof(line) - 1; ++i)
        line[pos++] = msg[i];

    line[pos] = '\0';

    int len = strlen(line);
    if (len > (int)width) len = (int)width;
    for (int x = 0; x < (int)width; ++x) {
        char c = (x < len) ? line[x] : ' ';
        terminal.put_at(c,
                        terminal.make_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE),
                        x, y);
    }
}

// Set the status message
void Editor::set_status_message(const char* msg) {
    strncpy(status_message, msg, sizeof(status_message) - 1);
    status_message[sizeof(status_message) - 1] = '\0';
}

// Render the editor view
void Editor::render() {
    int rows = terminal.get_vga_height() - 1;
    if (cursor_line < viewport_offset) {
        viewport_offset = cursor_line;
    } else if (cursor_line >= viewport_offset + rows) {
        viewport_offset = cursor_line - rows + 1;
    }

    size_t width = terminal.get_vga_width();
    int visible_rows = rows;

    for (int y = 0; y < visible_rows; ++y) {
        int idx = viewport_offset + y;
        if (idx < line_count) {
            draw_line(buffer[idx], y, idx == cursor_line);
        } else {
            for (size_t x = 0; x < width; ++x) {
                terminal.put_at(' ',
                                terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                                x, y);
            }
        }
    }

    draw_status_bar();

    int cy = cursor_line - viewport_offset;
    size_t cursor_x = (size_t)(cursor_col + PREFIX_ACTIVE_LEN);
    if (cursor_x >= width) cursor_x = width ? (width - 1) : 0;
    terminal.set_cursor(cy, cursor_x);
}

// Insert a character at the cursor position
void Editor::handle_char(char c) {
    char* line = buffer[cursor_line];
    int len = strlen(line);
    if (len >= EDITOR_LINE_LENGTH - 1) return;
    for (int i = len; i > cursor_col; --i) {
        line[i] = line[i - 1];
    }
    line[cursor_col++] = c;
    line[len + 1] = '\0';
}

// Handle Enter key: split line or process commands
void Editor::handle_enter() {
    char* ln = buffer[cursor_line];
    if (strcmp(ln, ".save") == 0) {
        set_status_message("Saved.");
        for (int i = cursor_line; i < line_count - 1; ++i)
            copy_line(buffer[i], buffer[i + 1]);
        line_count--; cursor_line = line_count - 1;
        cursor_col = strlen(buffer[cursor_line]);
        exit(true);
        return;
    }
    if (strcmp(ln, ".exit") == 0) {
        set_status_message("Exited.");
        exit(false);
        return;
    }
    if (line_count >= EDITOR_MAX_LINES) return;
    int len = strlen(ln);
    char right[EDITOR_LINE_LENGTH];
    int right_len = len - cursor_col;
    memcpy(right, ln + cursor_col, right_len);
    right[right_len] = '\0';
    ln[cursor_col] = '\0';

    for (int i = line_count; i > cursor_line + 1; --i)
        copy_line(buffer[i], buffer[i - 1]);
    copy_line(buffer[cursor_line + 1], right);

    line_count++;
    cursor_line++;
    cursor_col = 0;
}

// Handle Backspace key
void Editor::handle_backspace() {
    char* ln = buffer[cursor_line];
    int len = strlen(ln);
    if (cursor_col > 0) {
        for (int i = cursor_col - 1; i < len; ++i)
            ln[i] = ln[i + 1];
        cursor_col--;
    } else if (cursor_line > 0) {
        char* prev = buffer[cursor_line - 1];
        int prev_len = strlen(prev);
        int space = EDITOR_LINE_LENGTH - 1 - prev_len;
        int cp = (len < space ? len : space);
        if (cp > 0) {
            memcpy(prev + prev_len, ln, cp);
            prev[prev_len + cp] = '\0';
        }
        for (int i = cursor_line + 1; i < line_count; ++i)
            copy_line(buffer[i - 1], buffer[i]);
        line_count--;
        cursor_line--;
        cursor_col = prev_len;
    }
}

// Handle arrow key navigation
void Editor::handle_arrows(keyboard_event ke) {
    if (ke.scancode == KEY_UP_ARROW) {
        if (cursor_line > 0) {
            cursor_line--;
            int l = strlen(buffer[cursor_line]);
            if (cursor_col > l) cursor_col = l;
        }
    } else if (ke.scancode == KEY_DOWN_ARROW) {
        if (cursor_line < line_count - 1) {
            cursor_line++;
            int l = strlen(buffer[cursor_line]);
            if (cursor_col > l) cursor_col = l;
        } else if (cursor_line == line_count - 1 && line_count < EDITOR_MAX_LINES) {
            buffer[line_count][0] = '\0';
            line_count++;
            cursor_line++;
            cursor_col = 0;
        }
    } else if (ke.scancode == KEY_LEFT_ARROW) {
        if (cursor_col > 0) {
            cursor_col--;
        } else if (cursor_line > 0) {
            cursor_line--;
            cursor_col = strlen(buffer[cursor_line]);
        }
    } else if (ke.scancode == KEY_RIGHT_ARROW) {
        int l = strlen(buffer[cursor_line]);
        if (cursor_col < l) {
            cursor_col++;
        } else if (cursor_line < line_count - 1) {
            cursor_line++;
            cursor_col = 0;
        }
    }
}

// Handle a keyboard event in the editor
void Editor::handle_key(keyboard_event ke) {
    if (ke.release) {
        return;
    }
    if (ke.backspace) {
        handle_backspace();
    } else if (ke.enter) {
        handle_enter();
        if (!active) return;
    } else if (ke.scancode == KEY_UP_ARROW ||
               ke.scancode == KEY_DOWN_ARROW ||
               ke.scancode == KEY_LEFT_ARROW ||
               ke.scancode == KEY_RIGHT_ARROW) {
        handle_arrows(ke);
    } else {
        char c = kb_to_ascii(ke);
        if (c) handle_char(c);
    }
    render();
}

// Editor interface functions
void editor_start(const char* path) {
    editor_instance.start(path);
}
bool editor_is_active() {
    return editor_instance.is_active();
}
void editor_handle_key(keyboard_event ke) {
    editor_instance.handle_key(ke);
}

extern "C" void editor_entry() {
    printf("[editor] entry\n");
    Process* proc = scheduler_current_process();
    if (proc) {
        scheduler_set_foreground(proc);
    }

    // Use the safe copied params
    const char* target = s_path[0] ? s_path : "/untitled";
    printf("[editor] starting file '%s'\n", target);
    editor_start(target);
    IOEvent io_event;
    while (editor_is_active()) {
        if (!process_poll_event(&io_event)) {
            if (!process_wait_event(&io_event)) {
                continue;
            }
        }
        if (io_event.type == EVENT_PROCESS) {
            continue;
        }
        if (io_event.type == EVENT_KEYBOARD) {
            editor_handle_key(io_event.data.keyboard);
        }
    }
    printf("[editor] exit loop\n");
    process_exit(0);
    while (1) asm volatile("hlt");
}

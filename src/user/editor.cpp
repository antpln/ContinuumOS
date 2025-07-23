
#include <kernel/editor.h>
#include <stdio.h>
#include <string.h>
#include <kernel/heap.h>
#include <kernel/vga.h>
#include <kernel/scheduler.h>
#include <kernel/process.h>
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

// Copy a line with null-termination
static inline void copy_line(char* dst, const char* src) {
    strncpy(dst, src, EDITOR_LINE_LENGTH);
    dst[EDITOR_LINE_LENGTH - 1] = '\0';
}

// Start editing a file
void Editor::start(const char* filename, FSNode* current_dir) {
    active = true;
    status_message[0] = '\0';

    this->current_dir = current_dir;
    strncpy(this->filename, filename, sizeof(this->filename) - 1);
    this->filename[sizeof(this->filename) - 1] = '\0';

    FSNode* file = fs_find_child(current_dir, this->filename);
    if (file && file->type == FS_FILE && file->data && file->size > 0) {
        size_t size = file->size;
        uint8_t* data = file->data;
        int lines = 0;
        size_t pos = 0;

        while (pos < size && lines < EDITOR_MAX_LINES) {
            size_t start = pos;
            while (pos < size && data[pos] != '\n') pos++;
            size_t len = pos - start;
            if (len >= EDITOR_LINE_LENGTH) len = EDITOR_LINE_LENGTH - 1;
            memcpy(buffer[lines], data + start, len);
            buffer[lines][len] = '\0';
            lines++; pos++;
        }
        if (lines == 0) {
            buffer[0][0] = '\0';
            lines = 1;
        }
        line_count = lines;
    } else {
        line_count = 1;
        buffer[0][0] = '\0';
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
        if (!current_dir) {
            printf("Error: no directory to save in.\n");
            return;
        }
        FSNode* file = fs_find_child(current_dir, filename);
        if (!file) {
            file = fs_create_node(filename, FS_FILE);
            if (file) fs_add_child(current_dir, file);
        }
        if (file && file->type == FS_FILE) {
            while (line_count > 1 && strlen(buffer[line_count - 1]) == 0) {
                --line_count;
            }
            size_t total = 0;
            for (int i = 0; i < line_count; ++i)
                total += strlen(buffer[i]) + 1;

            if (file->data) kfree(file->data);
            uint8_t* data = static_cast<uint8_t*>(kmalloc(total + 1));
            if (!data) {
                printf("Memory allocation failed.\n");
                return;
            }
            file->data = data;
            size_t pos = 0;
            for (int i = 0; i < line_count; ++i) {
                size_t len = strlen(buffer[i]);
                memcpy(data + pos, buffer[i], len);
                pos += len;
                data[pos++] = '\n';
            }
            data[pos] = '\0';
            file->size = pos;
            printf("File '%s' saved.\n", filename);
        } else {
            printf("Error: could not create or write file '%s'.\n", filename);
        }
    } else {
        printf("Edit aborted.\n");
    }
    active = false;
    // Clear the status-bar row
    int y = terminal.get_vga_height() - 1;
    for (int x = 0; x < EDITOR_LINE_LENGTH; ++x) {
        terminal.put_at(' ',
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        x, y);
    }
    printf("nutshell> ");

    Process* proc = scheduler_current_process();
    if (proc)
        kill_process(proc);
}

// Draw a single line in the editor
void Editor::draw_line(const char* text, int y, bool is_active_line) {
    const char* prefix = is_active_line ? PREFIX_ACTIVE : PREFIX_INACTIVE;
    int prefix_len = is_active_line ? PREFIX_ACTIVE_LEN : PREFIX_INACTIVE_LEN;

    for (int i = 0; i < prefix_len; ++i) {
        terminal.put_at(prefix[i],
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        i, y);
    }

    int len = strlen(text);
    for (int i = 0; i < len; ++i) {
        char c = text[i] ? text[i] : ' ';
        int x = prefix_len + i;
        auto color = (is_active_line && i == cursor_col)
                   ? terminal.make_color(VGA_COLOR_BLACK, VGA_COLOR_WHITE)
                   : terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        terminal.put_at(c, color, x, y);
    }

    for (int x = prefix_len + len; x < EDITOR_LINE_LENGTH; ++x) {
        terminal.put_at(' ',
                        terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                        x, y);
    }
}

// Draw the status bar at the bottom
void Editor::draw_status_bar() {
    int y = terminal.get_vga_height() - 1;
    char line[EDITOR_LINE_LENGTH];
    int pos = 0;

    // "editing: filename"
    const char* lbl = "editing: ";
    for (int i = 0; lbl[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = lbl[i];
    for (int i = 0; filename[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = filename[i];

    // "  |  Ln X/Y"
    const char* mid1 = "  |  Ln ";
    for (int i = 0; mid1[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = mid1[i];
    pos += uitoa(cursor_line + 1, &line[pos], EDITOR_LINE_LENGTH - pos);
    if (pos < EDITOR_LINE_LENGTH - 1)
        line[pos++] = '/';
    pos += uitoa(line_count, &line[pos], EDITOR_LINE_LENGTH - pos);

    // "  Col Z"
    const char* mid2 = "  Col ";
    for (int i = 0; mid2[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = mid2[i];
    pos += uitoa(cursor_col + 1, &line[pos], EDITOR_LINE_LENGTH - pos);

    // "  |  MESSAGE"
    const char* mid3 = "  |  ";
    for (int i = 0; mid3[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = mid3[i];
    const char* msg = status_message[0] ? status_message : "EDITING";
    for (int i = 0; msg[i] && pos < EDITOR_LINE_LENGTH - 1; ++i)
        line[pos++] = msg[i];

    line[pos] = '\0';

    int len = strlen(line);
    for (int x = 0; x < EDITOR_LINE_LENGTH; ++x) {
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

    for (int y = 0; y < rows; ++y) {
        int idx = viewport_offset + y;
        if (idx < line_count) {
            draw_line(buffer[idx], y, idx == cursor_line);
        } else {
            for (int x = 0; x < EDITOR_LINE_LENGTH; ++x) {
                terminal.put_at(' ',
                                terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK),
                                x, y);
            }
        }
    }

    draw_status_bar();

    int cy = cursor_line - viewport_offset;
    terminal.set_cursor(cy, cursor_col + PREFIX_ACTIVE_LEN);
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
void editor_start(const char* filename, FSNode* current_dir) {
    editor_instance.start(filename, current_dir);
}
bool editor_is_active() {
    return editor_instance.is_active();
}
void editor_handle_key(keyboard_event ke) {
    editor_instance.handle_key(ke);
}

// Globals used to pass parameters from the shell
const char* editor_filename_global = nullptr;
FSNode* editor_dir_global = nullptr;

extern "C" void editor_entry() {
    Process* proc = scheduler_current_process();
    if (proc)
        register_keyboard_handler(proc, editor_handle_key);
    editor_start(editor_filename_global, editor_dir_global);
    while (editor_is_active()) {
        asm volatile("hlt");
    }
    if (proc)
        kill_process(proc);
    while (1) asm volatile("hlt");
}

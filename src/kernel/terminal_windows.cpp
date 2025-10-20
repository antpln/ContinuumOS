#include <kernel/terminal_windows.h>

#include <kernel/framebuffer.h>
#include <kernel/font8x16.h>
#include <kernel/gui.h>
#include <kernel/process.h>
#include <kernel/vga.h>
#include <kernel/debug.h>
#include <kernel/serial.h>
#include <kernel/scheduler.h>

#include <string.h>

namespace terminal_windows
{
namespace
{
struct DirtyRegion
{
    size_t min_row;
    size_t max_row;
    bool full_refresh;
};

struct Window
{
    bool in_use = false;
    Process *owner = nullptr;
    Terminal::Snapshot snapshot{};
    uint32_t frame_x = 0;
    uint32_t frame_y = 0;
    DirtyRegion dirty{};
};

struct RGB
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

constexpr size_t MAX_WINDOWS = 8;
constexpr uint32_t FRAME_BORDER = 2;
constexpr uint32_t TITLE_BAR_HEIGHT = 24;
constexpr uint32_t CONTENT_PADDING_X = 12;
constexpr uint32_t CONTENT_PADDING_BOTTOM = 12;
constexpr uint32_t INITIAL_FRAME_X = 40;
constexpr uint32_t INITIAL_FRAME_Y = 72;
constexpr uint32_t CASCADE_STEP_X = 28;
constexpr uint32_t CASCADE_STEP_Y = 28;

constexpr uint32_t CLOSE_BUTTON_SIZE = 14;
constexpr uint32_t CLOSE_BUTTON_MARGIN = 8;

constexpr RGB CLOSE_BUTTON_BG{200, 60, 70};
constexpr RGB CLOSE_BUTTON_FG{240, 240, 240};
constexpr RGB FRAME_BORDER_HIGHLIGHT{38, 44, 58};
constexpr RGB TITLE_TOP_HIGHLIGHT{130, 150, 210};
constexpr RGB TITLE_BOTTOM_SHADOW{18, 20, 30};
constexpr RGB ACTIVE_ACCENT_GLOW{100, 160, 255};
constexpr RGB TITLE_TEXT_SHADOW{18, 24, 32};

constexpr RGB FRAME_BORDER_COLOR{18, 22, 30};
constexpr RGB FRAME_BACKGROUND_COLOR{30, 34, 46};
constexpr RGB TITLE_ACTIVE_TOP{82, 128, 204};
constexpr RGB TITLE_ACTIVE_BOTTOM{46, 78, 140};
constexpr RGB TITLE_INACTIVE_TOP{60, 66, 84};
constexpr RGB TITLE_INACTIVE_BOTTOM{42, 48, 62};
constexpr RGB TITLE_TEXT_COLOR{236, 240, 248};
constexpr RGB TITLE_BOTTOM_LINE{28, 34, 46};
constexpr RGB CONTENT_BACKGROUND_COLOR{16, 20, 28};
constexpr RGB VGA_PALETTE[16] = {
    {0, 0, 0},        // black
    {0, 0, 170},      // blue
    {0, 170, 0},      // green
    {0, 170, 170},    // cyan
    {170, 0, 0},      // red
    {170, 0, 170},    // magenta
    {170, 85, 0},     // brown
    {170, 170, 170},  // light grey
    {85, 85, 85},     // dark grey
    {85, 85, 255},    // light blue
    {85, 255, 85},    // light green
    {85, 255, 255},   // light cyan
    {255, 85, 85},    // light red
    {255, 85, 255},   // light magenta
    {255, 255, 85},   // yellow
    {255, 255, 255}   // white
};
constexpr char FALLBACK_GLYPH = '?';

Window g_windows[MAX_WINDOWS];
size_t g_window_count = 0;
int g_active_slot = -1;

size_t g_z_order[MAX_WINDOWS];
size_t g_z_count = 0;

uint32_t g_content_width = 0;
uint32_t g_content_height = 0;
uint32_t g_frame_width = 0;
uint32_t g_frame_height = 0;
uint32_t g_content_offset_x = 0;
uint32_t g_content_offset_y = 0;
bool g_geometry_ready = false;

Terminal::Snapshot g_blank_snapshot{};
bool g_blank_ready = false;
#ifdef DEBUG
bool g_logged_sample_pixel = false;
#endif

struct WindowHitResult
{
    int slot = -1;
    bool on_title_bar = false;
    bool on_close_button = false;
    uint32_t local_x = 0;
    uint32_t local_y = 0;
};

bool g_dragging_window = false;
int g_drag_slot = -1;
int32_t g_drag_offset_x = 0;
int32_t g_drag_offset_y = 0;

constexpr int INVALID_SLOT = -1;

bool window_slot_valid(size_t slot)
{
    return slot < MAX_WINDOWS && g_windows[slot].in_use;
}

size_t find_stack_position(size_t slot)
{
    for (size_t i = 0; i < g_z_count; ++i)
    {
        if (g_z_order[i] == slot)
        {
            return i;
        }
    }
    return MAX_WINDOWS;
}

DirtyRegion make_empty_dirty_region();
void reset_dirty(DirtyRegion &region);

void push_slot_to_top(size_t slot)
{
    if (slot >= MAX_WINDOWS)
    {
        return;
    }

    size_t position = find_stack_position(slot);
    if (position == MAX_WINDOWS)
    {
        if (g_z_count < MAX_WINDOWS)
        {
            g_z_order[g_z_count++] = slot;
        }
        return;
    }

    for (size_t i = position; i + 1 < g_z_count; ++i)
    {
        g_z_order[i] = g_z_order[i + 1];
    }
    g_z_order[g_z_count - 1] = slot;
}

void remove_slot_from_stack(size_t slot)
{
    size_t position = find_stack_position(slot);
    if (position == MAX_WINDOWS)
    {
        return;
    }

    for (size_t i = position; i + 1 < g_z_count; ++i)
    {
        g_z_order[i] = g_z_order[i + 1];
    }
    if (g_z_count > 0)
    {
        --g_z_count;
    }
}

size_t allocate_window_slot()
{
    for (size_t i = 0; i < MAX_WINDOWS; ++i)
    {
        if (!g_windows[i].in_use)
        {
            g_windows[i] = Window{};
            g_windows[i].in_use = true;
            reset_dirty(g_windows[i].dirty);
            ++g_window_count;
            return i;
        }
    }
    return MAX_WINDOWS;
}

void release_window_slot(size_t slot)
{
    if (!window_slot_valid(slot))
    {
        return;
    }
    g_windows[slot] = Window{};
    g_windows[slot].in_use = false;
    reset_dirty(g_windows[slot].dirty);
    if (g_window_count > 0)
    {
        --g_window_count;
    }
}

DirtyRegion make_empty_dirty_region()
{
    return DirtyRegion{Terminal::VGA_HEIGHT, 0, false};
}

void reset_dirty(DirtyRegion &region)
{
    region = make_empty_dirty_region();
}

void stop_dragging();

void close_window_slot(size_t slot, Terminal &terminal)
{
    if (!window_slot_valid(slot))
    {
        return;
    }

    if (g_dragging_window && g_drag_slot == static_cast<int>(slot))
    {
        stop_dragging();
    }

    remove_slot_from_stack(slot);
    release_window_slot(slot);

    if (g_z_count == 0)
    {
        g_active_slot = INVALID_SLOT;
    }
    else
    {
        g_active_slot = static_cast<int>(g_z_order[g_z_count - 1]);
    }

    gui::draw_workspace(terminal);
}

bool dirty_region_has_updates(const DirtyRegion &region)
{
    return region.full_refresh || region.min_row <= region.max_row;
}

void mark_full_dirty(DirtyRegion &region)
{
    region.full_refresh = true;
    region.min_row = 0;
    region.max_row = Terminal::VGA_HEIGHT > 0 ? Terminal::VGA_HEIGHT - 1 : 0;
}

void mark_row_dirty(DirtyRegion &region, size_t row)
{
    if (region.full_refresh || row >= Terminal::VGA_HEIGHT)
    {
        return;
    }

    if (region.min_row > region.max_row)
    {
        region.min_row = row;
        region.max_row = row;
        return;
    }

    if (row < region.min_row)
    {
        region.min_row = row;
    }
    if (row > region.max_row)
    {
        region.max_row = row;
    }
}

uint32_t pack(const RGB &color)
{
    return framebuffer::pack_color(color.r, color.g, color.b);
}

void stop_dragging()
{
    g_dragging_window = false;
    g_drag_slot = INVALID_SLOT;
    g_drag_offset_x = 0;
    g_drag_offset_y = 0;
}

WindowHitResult hit_test_window(uint32_t x, uint32_t y)
{
    WindowHitResult result{};
    if (!framebuffer::is_available() || !g_geometry_ready || g_z_count == 0)
    {
        return result;
    }

    auto test_slot = [&](size_t slot) -> bool {
        if (!window_slot_valid(slot))
        {
            return false;
        }

        const Window &window = g_windows[slot];

        const uint32_t frame_x = window.frame_x;
        const uint32_t frame_y = window.frame_y;

        if (x < frame_x || y < frame_y)
        {
            return false;
        }

        const uint32_t rel_x = x - frame_x;
        const uint32_t rel_y = y - frame_y;

        if (rel_x >= g_frame_width || rel_y >= g_frame_height)
        {
            return false;
        }

        const uint32_t inner_width = g_frame_width > 2U * FRAME_BORDER ? g_frame_width - 2U * FRAME_BORDER : 0U;
        const uint32_t close_x_start = FRAME_BORDER + (inner_width > CLOSE_BUTTON_SIZE + CLOSE_BUTTON_MARGIN
                                                           ? inner_width - CLOSE_BUTTON_MARGIN - CLOSE_BUTTON_SIZE
                                                           : inner_width);
        const uint32_t close_y_start = FRAME_BORDER + (TITLE_BAR_HEIGHT > CLOSE_BUTTON_SIZE
                                                            ? (TITLE_BAR_HEIGHT - CLOSE_BUTTON_SIZE) / 2U
                                                            : 0U);
        if (inner_width > CLOSE_BUTTON_SIZE + CLOSE_BUTTON_MARGIN &&
            rel_x >= close_x_start && rel_x < close_x_start + CLOSE_BUTTON_SIZE &&
            rel_y >= close_y_start && rel_y < close_y_start + CLOSE_BUTTON_SIZE)
        {
            result.slot = static_cast<int>(slot);
            result.on_close_button = true;
            result.on_title_bar = false;
            result.local_x = rel_x;
            result.local_y = rel_y;
            return true;
        }

        const uint32_t title_region = FRAME_BORDER + TITLE_BAR_HEIGHT;

        result.slot = static_cast<int>(slot);
        result.on_close_button = false;
        result.on_title_bar = (rel_y < title_region);
        result.local_x = rel_x;
        result.local_y = rel_y;
        return true;
    };

    if (g_active_slot >= 0 && test_slot(static_cast<size_t>(g_active_slot)))
    {
        return result;
    }

    for (size_t i = g_z_count; i > 0; --i)
    {
        size_t slot = g_z_order[i - 1];
        if (static_cast<int>(slot) == g_active_slot)
        {
            continue;
        }
        if (test_slot(slot))
        {
            return result;
        }
    }

    return result;
}

uint32_t vga_to_rgb(uint8_t index)
{
    index &= 0x0F;
    const RGB &rgb = VGA_PALETTE[index];
    return framebuffer::pack_color(rgb.r, rgb.g, rgb.b);
}

uint32_t content_origin_x(const Window &window);
uint32_t content_origin_y(const Window &window);
WindowHitResult hit_test_window(uint32_t x, uint32_t y);
void stop_dragging();

void ensure_geometry(Terminal &terminal)
{
    if (g_geometry_ready || !framebuffer::is_available())
    {
        return;
    }

    g_content_width = static_cast<uint32_t>(terminal.pixel_width());
    g_content_height = static_cast<uint32_t>(terminal.pixel_height());
    g_content_offset_x = FRAME_BORDER + CONTENT_PADDING_X;
    g_content_offset_y = FRAME_BORDER + TITLE_BAR_HEIGHT;
    g_frame_width = g_content_width + 2U * FRAME_BORDER + 2U * CONTENT_PADDING_X;
    g_frame_height = g_content_height + 2U * FRAME_BORDER + TITLE_BAR_HEIGHT + CONTENT_PADDING_BOTTOM;
    g_geometry_ready = true;
}

void build_blank_snapshot(Terminal &terminal)
{
    if (g_blank_ready)
    {
        return;
    }

    uint8_t default_color = terminal.make_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    Terminal::Snapshot snapshot{};
    for (size_t y = 0; y < Terminal::VGA_HEIGHT; ++y)
    {
        for (size_t x = 0; x < Terminal::VGA_WIDTH; ++x)
        {
            snapshot.characters[y][x] = ' ';
            snapshot.colors[y][x] = default_color;
        }
    }
    snapshot.row = 0;
    snapshot.column = 0;
    snapshot.color = default_color;
    snapshot.cursor_row = 0;
    snapshot.cursor_column = 0;
    snapshot.cursor_active = false;

    g_blank_snapshot = snapshot;
    g_blank_ready = true;
}

void clear_snapshot_row(Terminal::Snapshot &snapshot, size_t row, DirtyRegion &dirty)
{
    if (row >= Terminal::VGA_HEIGHT)
    {
        return;
    }

    for (size_t x = 0; x < Terminal::VGA_WIDTH; ++x)
    {
        snapshot.characters[row][x] = ' ';
        snapshot.colors[row][x] = snapshot.color;
    }

    mark_row_dirty(dirty, row);
}

void snapshot_scroll(Terminal::Snapshot &snapshot, DirtyRegion &dirty)
{
    for (size_t y = 0; y + 1 < Terminal::VGA_HEIGHT; ++y)
    {
        for (size_t x = 0; x < Terminal::VGA_WIDTH; ++x)
        {
            snapshot.characters[y][x] = snapshot.characters[y + 1][x];
            snapshot.colors[y][x] = snapshot.colors[y + 1][x];
        }
    }

    mark_full_dirty(dirty);
    clear_snapshot_row(snapshot, Terminal::VGA_HEIGHT - 1, dirty);
    snapshot.row = Terminal::VGA_HEIGHT - 1;
    snapshot.column = 0;
    snapshot.cursor_row = snapshot.row;
    snapshot.cursor_column = snapshot.column;
    snapshot.cursor_active = false;
}

void snapshot_new_line(Terminal::Snapshot &snapshot, DirtyRegion &dirty)
{
    snapshot.column = 0;
    if (++snapshot.row >= Terminal::VGA_HEIGHT)
    {
        snapshot_scroll(snapshot, dirty);
    }
    else
    {
#ifdef DEBUG
        g_logged_sample_pixel = false;
#endif
        snapshot.cursor_row = snapshot.row;
        snapshot.cursor_column = snapshot.column;
    }
}

void snapshot_backspace(Terminal::Snapshot &snapshot, DirtyRegion &dirty)
{
    if (snapshot.column > 0)
    {
        --snapshot.column;
    }
    else if (snapshot.row > 0)
    {
        --snapshot.row;
        snapshot.column = Terminal::VGA_WIDTH - 1;
    }

    snapshot.characters[snapshot.row][snapshot.column] = ' ';
    snapshot.colors[snapshot.row][snapshot.column] = snapshot.color;
    snapshot.cursor_row = snapshot.row;
    snapshot.cursor_column = snapshot.column;
    snapshot.cursor_active = false;

    mark_row_dirty(dirty, snapshot.row);
}

void snapshot_put_char(Terminal::Snapshot &snapshot, char c, DirtyRegion &dirty)
{
    if (c == '\r')
    {
        snapshot.column = 0;
        snapshot.cursor_column = 0;
        snapshot.cursor_row = snapshot.row;
        snapshot.cursor_active = false;
        return;
    }

    if (c == '\n')
    {
        snapshot_new_line(snapshot, dirty);
        return;
    }

    if (c == '\b')
    {
        snapshot_backspace(snapshot, dirty);
        return;
    }

    snapshot.characters[snapshot.row][snapshot.column] = c;
    snapshot.colors[snapshot.row][snapshot.column] = snapshot.color;
    mark_row_dirty(dirty, snapshot.row);

    if (++snapshot.column >= Terminal::VGA_WIDTH)
    {
        snapshot_new_line(snapshot, dirty);
    }
    else
    {
        snapshot.cursor_row = snapshot.row;
        snapshot.cursor_column = snapshot.column;
        snapshot.cursor_active = false;
    }
}

void snapshot_write(Terminal::Snapshot &snapshot, const char *text, size_t length, DirtyRegion &dirty)
{
    for (size_t i = 0; i < length; ++i)
    {
        snapshot_put_char(snapshot, text[i], dirty);
    }
}

void draw_snapshot_contents(const Window &window, bool active, size_t start_row = 0, size_t end_row = Terminal::VGA_HEIGHT - 1)
{
    if (!framebuffer::is_available())
    {
        return;
    }

    if (start_row >= Terminal::VGA_HEIGHT)
    {
        return;
    }

    if (end_row >= Terminal::VGA_HEIGHT)
    {
        end_row = Terminal::VGA_HEIGHT - 1;
    }

    if (start_row > end_row)
    {
        return;
    }

    const uint32_t base_x = content_origin_x(window);
    const uint32_t base_y = content_origin_y(window);
    const uint32_t default_bg = pack(CONTENT_BACKGROUND_COLOR);

    for (size_t row = start_row; row <= end_row; ++row)
    {
        const uint32_t cell_y = base_y + static_cast<uint32_t>(row) * gui::FONT_HEIGHT;
        if (cell_y >= framebuffer::info().height)
        {
            break;
        }

        for (size_t col = 0; col < Terminal::VGA_WIDTH; ++col)
        {
            const uint32_t cell_x = base_x + static_cast<uint32_t>(col) * gui::FONT_WIDTH;
            if (cell_x >= framebuffer::info().width)
            {
                break;
            }

            const char raw_char = window.snapshot.characters[row][col];
            const uint8_t attrib = window.snapshot.colors[row][col];
            const uint8_t fg_index = attrib & 0x0F;
            const uint8_t bg_index = static_cast<uint8_t>((attrib >> 4) & 0x0F);

            const uint32_t bg_color = (bg_index == VGA_COLOR_BLACK) ? default_bg : vga_to_rgb(bg_index);
            framebuffer::fill_rect(cell_x, cell_y, gui::FONT_WIDTH, gui::FONT_HEIGHT, bg_color);

            char glyph_char = raw_char;
            if (glyph_char < 32 || glyph_char > 126)
            {
                glyph_char = FALLBACK_GLYPH;
            }
            const uint8_t *glyph = gui::glyph_for(glyph_char);
            const uint32_t fg_color = vga_to_rgb(fg_index);
            framebuffer::draw_mono_bitmap(cell_x,
                                          cell_y,
                                          gui::FONT_WIDTH,
                                          gui::FONT_HEIGHT,
                                          glyph,
                                          1,
                                          fg_color,
                                          0,
                                          true);

#ifdef DEBUG
            if (!g_logged_sample_pixel && raw_char != ' ')
            {
                bool found_fg = false;
                for (uint32_t dy = 0; dy < gui::FONT_HEIGHT && !found_fg; ++dy)
                {
                    for (uint32_t dx = 0; dx < gui::FONT_WIDTH; ++dx)
                    {
                        const uint32_t sample = framebuffer::peek_pixel(cell_x + dx, cell_y + dy);
                        if (sample == fg_color)
                        {
                            found_fg = true;
                            break;
                        }
                    }
                }

                g_logged_sample_pixel = true;
            }
#endif
        }
    }

    if (active && window.snapshot.cursor_active)
    {
        const uint32_t caret_x = base_x + static_cast<uint32_t>(window.snapshot.cursor_column) * gui::FONT_WIDTH;
        const uint32_t caret_y = base_y + static_cast<uint32_t>(window.snapshot.cursor_row) * gui::FONT_HEIGHT;
        framebuffer::fill_rect(caret_x, caret_y, 2, gui::FONT_HEIGHT, framebuffer::pack_color(240, 240, 255));
    }
}

void present_window_slot(size_t slot)
{
    if (!window_slot_valid(slot))
    {
        return;
    }

    Window &window = g_windows[slot];
    if (!dirty_region_has_updates(window.dirty))
    {
        return;
    }

    if (g_active_slot != static_cast<int>(slot))
    {
        reset_dirty(window.dirty);
        return;
    }

    gui::begin_window_redraw();
    if (window.dirty.full_refresh)
    {
        draw_snapshot_contents(window, true);
    }
    else
    {
        draw_snapshot_contents(window, true, window.dirty.min_row, window.dirty.max_row);
    }
    gui::end_window_redraw();
    reset_dirty(window.dirty);
}

void clamp_frame(uint32_t &frame_x, uint32_t &frame_y)
{
    if (!framebuffer::is_available() || !g_geometry_ready)
    {
        frame_x = 0;
        frame_y = 0;
        return;
    }

    const auto &fb = framebuffer::info();

    if (g_frame_width >= fb.width)
    {
        frame_x = 0;
    }
    else if (frame_x + g_frame_width > fb.width)
    {
        frame_x = fb.width - g_frame_width;
    }

    if (g_frame_height >= fb.height)
    {
        frame_y = 0;
    }
    else if (frame_y + g_frame_height > fb.height)
    {
        frame_y = fb.height - g_frame_height;
    }
}

uint32_t content_origin_x(const Window &window)
{
    return window.frame_x + g_content_offset_x;
}

uint32_t content_origin_y(const Window &window)
{
    return window.frame_y + g_content_offset_y;
}

int find_window_index(Process *proc)
{
    if (proc == nullptr)
    {
        return -1;
    }

    for (size_t i = 0; i < MAX_WINDOWS; ++i)
    {
        if (window_slot_valid(i) && g_windows[i].owner == proc)
        {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void draw_text(uint32_t x, uint32_t y, const char *text, uint32_t color, uint32_t max_width, bool drop_shadow = false)
{
    if (text == nullptr)
    {
        return;
    }

    uint32_t max_chars = max_width / gui::FONT_WIDTH;
    if (max_chars == 0)
    {
        return;
    }

    char buffer[128];
    size_t length = 0;
    while (text[length] != '\0' && length < sizeof(buffer) - 1 && length < max_chars)
    {
        buffer[length] = text[length];
        ++length;
    }
    buffer[length] = '\0';

    for (size_t i = 0; i < length; ++i)
    {
        const uint8_t *glyph = gui::glyph_for(buffer[i]);
        if (drop_shadow)
        {
            framebuffer::draw_mono_bitmap(x + 1,
                                          y + 1,
                                          gui::FONT_WIDTH,
                                          gui::FONT_HEIGHT,
                                          glyph,
                                          1,
                                          pack(TITLE_TEXT_SHADOW),
                                          0,
                                          true);
        }
        framebuffer::draw_mono_bitmap(x,
                                      y,
                                      gui::FONT_WIDTH,
                                      gui::FONT_HEIGHT,
                                      glyph,
                                      1,
                                      color,
                                      0,
                                      true);
        x += gui::FONT_WIDTH;
    }
}

void draw_window_frame(const Window &window, bool active)
{
    if (!framebuffer::is_available() || !g_geometry_ready)
    {
        return;
    }

    const uint32_t frame_x = window.frame_x;
    const uint32_t frame_y = window.frame_y;
    const uint32_t inner_x = frame_x + FRAME_BORDER;
    const uint32_t inner_y = frame_y + FRAME_BORDER;
    const uint32_t inner_width = g_frame_width > 2U * FRAME_BORDER ? g_frame_width - 2U * FRAME_BORDER : 0;
    const uint32_t inner_height = g_frame_height > 2U * FRAME_BORDER ? g_frame_height - 2U * FRAME_BORDER : 0;

    framebuffer::fill_rect(frame_x, frame_y, g_frame_width, g_frame_height, pack(FRAME_BORDER_COLOR));
    framebuffer::fill_rect(frame_x, frame_y, g_frame_width, 1, pack(FRAME_BORDER_HIGHLIGHT));

    if (inner_width == 0 || inner_height == 0)
    {
        return;
    }

    framebuffer::fill_rect(inner_x, inner_y, inner_width, inner_height, pack(FRAME_BACKGROUND_COLOR));

    const RGB top_color = active ? TITLE_ACTIVE_TOP : TITLE_INACTIVE_TOP;
    const RGB bottom_color = active ? TITLE_ACTIVE_BOTTOM : TITLE_INACTIVE_BOTTOM;
    const uint32_t title_height = TITLE_BAR_HEIGHT;
    const uint32_t title_y = inner_y;

    if (title_height > 0)
    {
        for (uint32_t row = 0; row < title_height && row < inner_height; ++row)
        {
            const uint32_t mix = title_height > 1 ? (row * 255U) / (title_height - 1U) : 0U;
            const uint32_t inv = 255U - mix;
            const uint8_t r = static_cast<uint8_t>((top_color.r * inv + bottom_color.r * mix) / 255U);
            const uint8_t g = static_cast<uint8_t>((top_color.g * inv + bottom_color.g * mix) / 255U);
            const uint8_t b = static_cast<uint8_t>((top_color.b * inv + bottom_color.b * mix) / 255U);
            framebuffer::fill_rect(inner_x, title_y + row, inner_width, 1, framebuffer::pack_color(r, g, b));
        }

        framebuffer::fill_rect(inner_x, title_y, inner_width, 1, pack(TITLE_TOP_HIGHLIGHT));
        framebuffer::fill_rect(inner_x, title_y + title_height - 1, inner_width, 1, pack(TITLE_BOTTOM_LINE));
        framebuffer::fill_rect(inner_x, title_y + title_height, inner_width, 1, pack(TITLE_BOTTOM_SHADOW));
    }

    if (active)
    {
        const uint32_t accent_height = title_height > 4 ? title_height - 4 : title_height;
        if (accent_height > 0)
        {
            framebuffer::fill_rect(frame_x + FRAME_BORDER,
                                   frame_y + FRAME_BORDER + 2,
                                   2,
                                   accent_height,
                                   pack(ACTIVE_ACCENT_GLOW));
        }
    }

    if (inner_width > CLOSE_BUTTON_SIZE + CLOSE_BUTTON_MARGIN)
    {
        const uint32_t close_x = inner_x + inner_width - CLOSE_BUTTON_MARGIN - CLOSE_BUTTON_SIZE;
        const uint32_t close_y = title_y + (title_height > CLOSE_BUTTON_SIZE ? (title_height - CLOSE_BUTTON_SIZE) / 2 : 0);
        for (uint32_t row = 0; row < CLOSE_BUTTON_SIZE; ++row)
        {
            const float t = CLOSE_BUTTON_SIZE > 1 ? static_cast<float>(row) / static_cast<float>(CLOSE_BUTTON_SIZE - 1U) : 0.0f;
            const uint8_t r = static_cast<uint8_t>(CLOSE_BUTTON_BG.r * (1.0f - t) + 120.0f * t);
            const uint8_t g = static_cast<uint8_t>(CLOSE_BUTTON_BG.g * (1.0f - t) + 30.0f * t);
            const uint8_t b = static_cast<uint8_t>(CLOSE_BUTTON_BG.b * (1.0f - t) + 40.0f * t);
            framebuffer::fill_rect(close_x,
                                   close_y + row,
                                   CLOSE_BUTTON_SIZE,
                                   1,
                                   framebuffer::pack_color(r, g, b));
        }

        framebuffer::fill_rect(close_x,
                               close_y,
                               CLOSE_BUTTON_SIZE,
                               1,
                               pack(TITLE_TOP_HIGHLIGHT));

        const uint32_t inset = CLOSE_BUTTON_SIZE / 4;
        const uint32_t button_size = CLOSE_BUTTON_SIZE - inset * 2;
        const uint32_t button_x = close_x + inset;
        const uint32_t button_y = close_y + inset;

        for (uint32_t i = 0; i < button_size; ++i)
        {
            const uint32_t x0 = button_x + i;
            const uint32_t y0 = button_y + i;
            framebuffer::fill_rect(x0, y0, 2, 1, pack(CLOSE_BUTTON_FG));

            const uint32_t x1 = button_x + (button_size - 1 - i);
            framebuffer::fill_rect(x1, y0, 2, 1, pack(CLOSE_BUTTON_FG));
        }
    }

    const char *title = "Terminal";
    if (window.owner != nullptr)
    {
        if (window.owner->name != nullptr)
        {
            title = window.owner->name;
        }
    }
    else
    {
        title = "System";
    }
    const uint32_t text_margin = 12;
    const uint32_t text_x = inner_x + text_margin;
    uint32_t text_y = title_y;
    if (title_height > gui::FONT_HEIGHT)
    {
        text_y += (title_height - gui::FONT_HEIGHT) / 2;
    }

    draw_text(text_x, text_y, title, pack(TITLE_TEXT_COLOR), inner_width - 2U * text_margin, true);
}

void render_window(Terminal &terminal, Window &window, bool active)
{
    (void)terminal;

    framebuffer::fill_rect(content_origin_x(window), content_origin_y(window), g_content_width, g_content_height, pack(CONTENT_BACKGROUND_COLOR));
    draw_window_frame(window, active);
    draw_snapshot_contents(window, active);
    reset_dirty(window.dirty);
}

void focus_window(size_t slot, Terminal &terminal)
{
    if (!window_slot_valid(slot))
    {
        return;
    }

    if (!g_geometry_ready)
    {
        ensure_geometry(terminal);
        if (!g_geometry_ready)
        {
            return;
        }
    }

    const int previous_active = g_active_slot;
    push_slot_to_top(slot);
    g_active_slot = static_cast<int>(slot);

    if (previous_active != g_active_slot)
    {
        draw_windows(terminal);
    }
}

void reset_windows()
{
    for (size_t i = 0; i < MAX_WINDOWS; ++i)
    {
        g_windows[i] = Window{};
        reset_dirty(g_windows[i].dirty);
        g_z_order[i] = 0;
    }
    g_window_count = 0;
    g_z_count = 0;
    g_active_slot = INVALID_SLOT;
    stop_dragging();
}
} // namespace

void init(Terminal &terminal, Process *initial_proc)
{
    reset_windows();
    ensure_geometry(terminal);
    build_blank_snapshot(terminal);

    if (initial_proc != nullptr)
    {
        request_new_window(terminal, initial_proc);
    }
}

void draw_windows(Terminal &terminal)
{
    gui::begin_window_redraw();

    debug("[GUI] draw_windows count=%u active_slot=%d",
          static_cast<unsigned>(g_window_count),
          g_active_slot);
    if (!framebuffer::is_available() || g_z_count == 0)
    {
        gui::end_window_redraw();
        return;
    }

    ensure_geometry(terminal);

    for (size_t i = 0; i < g_z_count; ++i)
    {
        size_t slot = g_z_order[i];
        if (!window_slot_valid(slot) || static_cast<int>(slot) == g_active_slot)
        {
            continue;
        }
        render_window(terminal, g_windows[slot], false);
    }

    if (g_active_slot >= 0 && window_slot_valid(static_cast<size_t>(g_active_slot)))
    {
        render_window(terminal, g_windows[g_active_slot], true);
    }

    gui::end_window_redraw();
}

void request_new_window(Terminal &terminal, Process *proc)
{
    debug("[GUI] request_new_window owner=%p", proc);
    if (!framebuffer::is_available())
    {
        return;
    }

    ensure_geometry(terminal);
    build_blank_snapshot(terminal);

    int existing = find_window_index(proc);
    if (existing >= 0)
    {
        if (proc != nullptr)
        {
            activate_process(proc, terminal);
        }
        else
        {
            focus_window(static_cast<size_t>(existing), terminal);
            draw_windows(terminal);
        }
        return;
    }

    if (g_window_count >= MAX_WINDOWS)
    {
        return;
    }

    size_t slot = allocate_window_slot();
    if (slot >= MAX_WINDOWS)
    {
        return;
    }

    Window &window = g_windows[slot];
    window.owner = proc;
    window.snapshot = g_blank_snapshot;
    reset_dirty(window.dirty);

    const size_t cascade_index = g_z_count;
    uint32_t frame_x = INITIAL_FRAME_X + static_cast<uint32_t>(cascade_index) * CASCADE_STEP_X;
    uint32_t frame_y = INITIAL_FRAME_Y + static_cast<uint32_t>(cascade_index) * CASCADE_STEP_Y;
    clamp_frame(frame_x, frame_y);
    window.frame_x = frame_x;
    window.frame_y = frame_y;

    debug("[GUI] window slot=%u frame=(%u,%u)", static_cast<unsigned>(slot), frame_x, frame_y);

    focus_window(slot, terminal);
}

void activate_process(Process *proc, Terminal &terminal)
{
    if (proc == nullptr)
    {
        return;
    }

    ensure_geometry(terminal);
    build_blank_snapshot(terminal);

    int index = find_window_index(proc);
    if (index < 0)
    {
        return;
    }

    focus_window(static_cast<size_t>(index), terminal);
}

void set_active_window_origin(Terminal &terminal, Process *proc, int32_t x, int32_t y)
{
    if (proc == nullptr || !framebuffer::is_available())
    {
        return;
    }

    ensure_geometry(terminal);

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return;
    }

    Window &window = g_windows[static_cast<size_t>(slot)];

    uint32_t desired_x = x < 0 ? 0U : static_cast<uint32_t>(x);
    uint32_t desired_y = y < 0 ? 0U : static_cast<uint32_t>(y);

    uint32_t frame_x = desired_x > g_content_offset_x ? desired_x - g_content_offset_x : 0U;
    uint32_t frame_y = desired_y > g_content_offset_y ? desired_y - g_content_offset_y : 0U;

    clamp_frame(frame_x, frame_y);

    window.frame_x = frame_x;
    window.frame_y = frame_y;

    draw_windows(terminal);
}

void on_process_exit(Process *proc, Terminal &terminal)
{
    if (proc == nullptr || !framebuffer::is_available())
    {
        return;
    }

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return;
    }

    close_window_slot(static_cast<size_t>(slot), terminal);
}

void write_text(Terminal &terminal, Process *proc, const char *text, size_t length)
{
    if (text == nullptr || length == 0)
    {
        return;
    }

    if (proc == nullptr || !framebuffer::is_available())
    {
        for (size_t i = 0; i < length; ++i)
        {
            terminal.putchar(text[i]);
        }
        return;
    }

    ensure_geometry(terminal);
    build_blank_snapshot(terminal);

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        request_new_window(terminal, proc);
        slot = find_window_index(proc);
        if (slot < 0)
        {
#ifdef DEBUG
            serial_printf("[TERMWIN] failed to create/find window for process %s\n",
                          proc->name ? proc->name : "(null)");
#endif
            return;
        }
    }

    Window &window = g_windows[static_cast<size_t>(slot)];
    if (window.snapshot.cursor_active)
    {
        mark_row_dirty(window.dirty, window.snapshot.cursor_row);
    }

    snapshot_write(window.snapshot, text, length, window.dirty);

    mark_row_dirty(window.dirty, window.snapshot.cursor_row);

    if (static_cast<int>(slot) == g_active_slot)
    {
        window_present(proc);
    }
}

void window_present(Process *proc)
{
    if (proc == nullptr)
    {
        return;
    }

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return;
    }

    present_window_slot(static_cast<size_t>(slot));
}

void window_put_char(Process *proc, size_t x, size_t y, char ch, uint8_t color)
{
    if (proc == nullptr)
    {
        return;
    }

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return;
    }

    if (x >= Terminal::VGA_WIDTH || y >= Terminal::VGA_HEIGHT)
    {
        return;
    }

    Window &window = g_windows[static_cast<size_t>(slot)];
    if (!window.in_use)
    {
        return;
    }

    if (window.snapshot.characters[y][x] == ch && window.snapshot.colors[y][x] == color)
    {
        return;
    }

    window.snapshot.characters[y][x] = ch;
    window.snapshot.colors[y][x] = color;
    mark_row_dirty(window.dirty, y);
}

void window_set_cursor(Process *proc, size_t row, size_t column, bool active)
{
    if (proc == nullptr)
    {
        return;
    }

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return;
    }

    Window &window = g_windows[static_cast<size_t>(slot)];
    size_t prev_row = window.snapshot.cursor_row;
    bool prev_active = window.snapshot.cursor_active;
    if (prev_active && prev_row < Terminal::VGA_HEIGHT)
    {
        mark_row_dirty(window.dirty, prev_row);
    }

    if (row >= Terminal::VGA_HEIGHT)
    {
        row = Terminal::VGA_HEIGHT - 1;
    }
    if (column >= Terminal::VGA_WIDTH)
    {
        column = Terminal::VGA_WIDTH - 1;
    }

    window.snapshot.cursor_row = row;
    window.snapshot.cursor_column = column;
    window.snapshot.cursor_active = active;

    if (active)
    {
        mark_row_dirty(window.dirty, row);
    }
}

bool window_get_cursor(Process *proc, size_t &row, size_t &column)
{
    row = 0;
    column = 0;
    if (proc == nullptr)
    {
        return false;
    }

    int slot = find_window_index(proc);
    if (slot < 0)
    {
        return false;
    }

    const Window &window = g_windows[static_cast<size_t>(slot)];
    row = window.snapshot.cursor_row;
    column = window.snapshot.cursor_column;
    return true;
}

bool handle_mouse_event(Terminal &terminal, const MouseEvent &event)
{
    if (!framebuffer::is_available())
    {
        return false;
    }

    ensure_geometry(terminal);

    const uint32_t cursor_x = event.x < 0 ? 0U : static_cast<uint32_t>(event.x);
    const uint32_t cursor_y = event.y < 0 ? 0U : static_cast<uint32_t>(event.y);

    const bool left_changed = (event.changed & MOUSE_BUTTON_LEFT) != 0;
    const bool left_pressed = left_changed && (event.buttons & MOUSE_BUTTON_LEFT);
    const bool left_released = left_changed && (event.buttons & MOUSE_BUTTON_LEFT) == 0;

    bool consumed = false;

    if (left_pressed)
    {
        WindowHitResult hit = hit_test_window(cursor_x, cursor_y);
        if (hit.slot >= 0 && window_slot_valid(static_cast<size_t>(hit.slot)))
        {
            Window &window = g_windows[static_cast<size_t>(hit.slot)];
            if (window.owner != nullptr)
            {
                if (scheduler_get_foreground() != window.owner)
                {
                    scheduler_set_foreground(window.owner);
                }
                else
                {
                    focus_window(static_cast<size_t>(hit.slot), terminal);
                }
            }
            else
            {
                focus_window(static_cast<size_t>(hit.slot), terminal);
            }

            if (hit.on_close_button)
            {
                stop_dragging();
                consumed = true;
                if (window.owner != nullptr)
                {
                    kill_process(window.owner);
                }
                else
                {
                    close_window_slot(static_cast<size_t>(hit.slot), terminal);
                }
                return true;
            }

            if (g_active_slot >= 0 && window_slot_valid(static_cast<size_t>(g_active_slot)))
            {
                Window &active_window = g_windows[static_cast<size_t>(g_active_slot)];
                if (hit.on_title_bar)
                {
                    g_dragging_window = true;
                    g_drag_slot = g_active_slot;
                    g_drag_offset_x = static_cast<int32_t>(cursor_x) - static_cast<int32_t>(active_window.frame_x);
                    g_drag_offset_y = static_cast<int32_t>(cursor_y) - static_cast<int32_t>(active_window.frame_y);
                }
                else
                {
                    stop_dragging();
                }
            }

            consumed = true;
        }
        else
        {
            stop_dragging();
        }
    }
    else if (left_released)
    {
        if (g_dragging_window)
        {
            consumed = true;
        }
        stop_dragging();
    }

    if (g_dragging_window && g_drag_slot >= 0 && window_slot_valid(static_cast<size_t>(g_drag_slot)))
    {
        Window &window = g_windows[static_cast<size_t>(g_drag_slot)];
        const uint32_t old_frame_x = window.frame_x;
        const uint32_t old_frame_y = window.frame_y;
        int32_t desired_x = static_cast<int32_t>(cursor_x) - g_drag_offset_x;
        int32_t desired_y = static_cast<int32_t>(cursor_y) - g_drag_offset_y;

        if (desired_x < 0)
        {
            desired_x = 0;
        }
        if (desired_y < 0)
        {
            desired_y = 0;
        }

        uint32_t frame_x = static_cast<uint32_t>(desired_x);
        uint32_t frame_y = static_cast<uint32_t>(desired_y);
        clamp_frame(frame_x, frame_y);

        if (frame_x != old_frame_x || frame_y != old_frame_y)
        {
            if (g_frame_width > 0 && g_frame_height > 0)
            {
                gui::fill_background_rect(old_frame_x, old_frame_y, g_frame_width, g_frame_height);
            }
            window.frame_x = frame_x;
            window.frame_y = frame_y;
            draw_windows(terminal);
        }
        return true;
    }

    return consumed;
}

} // namespace terminal_windows

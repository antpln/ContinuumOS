#include <kernel/app_registry.h>

#include <kernel/debug.h>
#include <kernel/vfs.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint8_t _binary_apps_editor_app_start[];
extern uint8_t _binary_apps_editor_app_end[];
extern uint8_t _binary_apps_hello_app_start[];
extern uint8_t _binary_apps_hello_app_end[];

#ifdef __cplusplus
}
#endif

namespace
{
struct BuiltinApp
{
    const char* path;
    const uint8_t* start;
    const uint8_t* end;
};

static const BuiltinApp g_builtin_apps[] = {
    {"/apps/editor.app", _binary_apps_editor_app_start, _binary_apps_editor_app_end},
    {"/apps/hello.app", _binary_apps_hello_app_start, _binary_apps_hello_app_end},
};

static void ensure_directory(const char* path)
{
    vfs_dirent_t info;
    if (vfs_stat(path, &info) == VFS_SUCCESS && info.type == VFS_TYPE_DIRECTORY)
    {
        return;
    }

    if (vfs_mkdir(path) != VFS_SUCCESS)
    {
        error("[apps] failed to create directory '%s'", path);
    }
}

static void install_app(const BuiltinApp& app)
{
    uintptr_t start = reinterpret_cast<uintptr_t>(app.start);
    uintptr_t end = reinterpret_cast<uintptr_t>(app.end);
    size_t size = (end > start) ? static_cast<size_t>(end - start) : 0;

    debug("[apps] installing '%s' start=%p end=%p size=%u",
          app.path,
          (const void*)app.start,
          (const void*)app.end,
          (unsigned)size);

    if (!app.start || size == 0)
    {
        error("[apps] invalid built-in app '%s'", app.path);
        return;
    }

    // Remove existing file if present
    vfs_remove(app.path);

    if (vfs_create(app.path) != VFS_SUCCESS)
    {
        error("[apps] failed to create '%s'", app.path);
        return;
    }

    vfs_file_t file;
    if (vfs_open(app.path, &file) != VFS_SUCCESS)
    {
        error("[apps] failed to open '%s' for writing", app.path);
        return;
    }

    size_t written = 0;
    while (written < size)
    {
        size_t remaining = size - written;
        int result = vfs_write(&file, app.start + written, remaining);
        if (result <= 0)
        {
            error("[apps] write error for '%s'", app.path);
            vfs_close(&file);
            return;
        }
        written += static_cast<size_t>(result);
    }

    vfs_close(&file);
    success("[apps] installed '%s' (%u bytes)", app.path, (unsigned)size);
}
} // namespace

void app_registry_init(void)
{
    ensure_directory("/apps");

    for (size_t i = 0; i < sizeof(g_builtin_apps) / sizeof(g_builtin_apps[0]); ++i)
    {
        install_app(g_builtin_apps[i]);
    }
}

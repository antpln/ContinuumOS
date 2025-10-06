#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>

// VFS constants
#define VFS_MAX_MOUNTS 8
#define VFS_MAX_PATH 256
#define VFS_MAX_NAME 64
#define VFS_MAX_OPEN_FILES 64

// VFS error codes
#define VFS_SUCCESS 0
#define VFS_ERROR -1
#define VFS_NOT_FOUND -2
#define VFS_NO_SPACE -3
#define VFS_INVALID_PATH -4
#define VFS_NOT_MOUNTED -5
#define VFS_ALREADY_MOUNTED -6

// File types
#define VFS_TYPE_FILE 1
#define VFS_TYPE_DIRECTORY 2

// Filesystem types
#define VFS_FS_RAMFS 1
#define VFS_FS_FAT32 2

// Forward declarations
struct vfs_mount;
struct vfs_file;
struct vfs_operations;

// VFS file handle
typedef struct vfs_file {
    uint32_t fs_handle;           // Filesystem-specific handle
    struct vfs_mount* mount;      // Mount point this file belongs to
    uint32_t position;            // Current position in file
    uint8_t in_use;              // Whether this handle is active
} vfs_file_t;

// Directory entry information
typedef struct {
    char name[VFS_MAX_NAME];
    uint8_t type;                // VFS_TYPE_FILE or VFS_TYPE_DIRECTORY
    uint32_t size;               // File size in bytes
} vfs_dirent_t;

// Filesystem operations interface
typedef struct vfs_operations {
    // File operations
    int (*open)(struct vfs_mount* mount, const char* path, vfs_file_t* file);
    int (*read)(vfs_file_t* file, void* buffer, size_t size);
    int (*write)(vfs_file_t* file, const void* buffer, size_t size);
    int (*seek)(vfs_file_t* file, uint32_t position);
    void (*close)(vfs_file_t* file);
    
    // Directory operations
    int (*readdir)(struct vfs_mount* mount, const char* path, vfs_dirent_t* entries, int max_entries);
    int (*mkdir)(struct vfs_mount* mount, const char* path);
    int (*rmdir)(struct vfs_mount* mount, const char* path);
    
    // File management
    int (*create)(struct vfs_mount* mount, const char* path);
    int (*remove)(struct vfs_mount* mount, const char* path);
    
    // Filesystem info
    int (*stat)(struct vfs_mount* mount, const char* path, vfs_dirent_t* info);
} vfs_operations_t;

// Mount point information
typedef struct vfs_mount {
    char mountpoint[VFS_MAX_PATH];  // Where this filesystem is mounted (e.g., "/", "/mnt/fat32")
    uint8_t fs_type;                // VFS_FS_RAMFS, VFS_FS_FAT32, etc.
    uint8_t device_id;              // For block device filesystems
    void* fs_data;                  // Filesystem-specific data
    vfs_operations_t* ops;          // Filesystem operations
    uint8_t mounted;                // Whether this mount is active
} vfs_mount_t;

// VFS global functions
int vfs_init(void);
int vfs_mount(const char* mountpoint, uint8_t fs_type, uint8_t device_id, vfs_operations_t* ops, void* fs_data);
int vfs_unmount(const char* mountpoint);

// VFS file operations
int vfs_open(const char* path, vfs_file_t* file);
int vfs_read(vfs_file_t* file, void* buffer, size_t size);
int vfs_write(vfs_file_t* file, const void* buffer, size_t size);
int vfs_seek(vfs_file_t* file, uint32_t position);
void vfs_close(vfs_file_t* file);

// VFS directory operations
int vfs_readdir(const char* path, vfs_dirent_t* entries, int max_entries);
int vfs_mkdir(const char* path);
int vfs_rmdir(const char* path);

// VFS file management
int vfs_create(const char* path);
int vfs_remove(const char* path);
int vfs_stat(const char* path, vfs_dirent_t* info);

// VFS utility functions
vfs_mount_t* vfs_find_mount(const char* path);
int vfs_resolve_path(const char* path, vfs_mount_t** mount, char* relative_path);
int vfs_list_mounts(void);
int vfs_normalize_path(const char* path, char* normalized_path);
int vfs_resolve_dots(const char* path, char* resolved_path);

// Current working directory
int vfs_chdir(const char* path);
const char* vfs_getcwd(void);

// Filesystem adapter functions
vfs_operations_t* ramfs_get_vfs_ops(void);
int ramfs_vfs_mount(const char* mountpoint);
vfs_operations_t* fat32_get_vfs_ops(void);
int fat32_vfs_mount(const char* mountpoint, uint8_t device_id);

#endif // VFS_H
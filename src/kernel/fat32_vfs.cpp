#include "kernel/vfs.h"
#include "kernel/fat32.h"
#include "kernel/heap.h"
#include "kernel/debug.h"
#include <stdio.h>
#include <string.h>

// FAT32 VFS adapter functions

static int fat32_vfs_open(vfs_mount_t* mount, const char* path, vfs_file_t* file) {
    (void)mount; // Mount info available if needed
    
    debug("[FAT32-VFS] Opening file: %s", path);
    
    // FAT32 currently only supports root directory files
    // Remove leading slash for FAT32 compatibility
    const char* filename = path;
    if (path[0] == '/') {
        filename = path + 1;
    }
    
    // Skip empty filename (root directory)
    if (filename[0] == '\0') {
        error("[FAT32-VFS] Cannot open root directory as file");
        return VFS_ERROR;
    }
    
    int fat32_fd = fat32_open(filename);
    if (fat32_fd < 0) {
        error("[FAT32-VFS] Failed to open file: %s", filename);
        return VFS_NOT_FOUND;
    }
    
    // Store FAT32 file descriptor as handle
    file->fs_handle = (uint32_t)fat32_fd;
    file->position = 0;
    file->in_use = 1;
    
    success("[FAT32-VFS] Successfully opened file: %s (fd=%d)", filename, fat32_fd);
    return VFS_SUCCESS;
}

static int fat32_vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }
    
    int fat32_fd = (int)file->fs_handle;
    int bytes_read = fat32_read(fat32_fd, buffer, size);
    
    if (bytes_read >= 0) {
        file->position += bytes_read;
        debug("[FAT32-VFS] Read %d bytes from FAT32 file", bytes_read);
    } else {
        error("[FAT32-VFS] Failed to read from FAT32 file");
    }
    
    return bytes_read;
}

static int fat32_vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    (void)file;
    (void)buffer;
    (void)size;
    
    error("[FAT32-VFS] Write not supported (read-only filesystem)");
    return VFS_ERROR;
}

static int fat32_vfs_seek(vfs_file_t* file, uint32_t position) {
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }
    
    int fat32_fd = (int)file->fs_handle;
    int result = fat32_seek(fat32_fd, position);
    
    if (result == 0) {
        file->position = position;
        debug("[FAT32-VFS] Seeked to position %u", position);
        return VFS_SUCCESS;
    } else {
        error("[FAT32-VFS] Failed to seek to position %u", position);
        return VFS_ERROR;
    }
}

static void fat32_vfs_close(vfs_file_t* file) {
    if (file && file->in_use) {
        int fat32_fd = (int)file->fs_handle;
        fat32_close(fat32_fd);
        
        debug("[FAT32-VFS] Closed FAT32 file (fd=%d)", fat32_fd);
        file->in_use = 0;
        file->fs_handle = 0;
        file->position = 0;
    }
}

static int fat32_vfs_readdir(vfs_mount_t* mount, const char* path, vfs_dirent_t* entries, int max_entries) {
    (void)mount; // Mount info available if needed
    
    debug("[FAT32-VFS] Reading directory: %s", path);
    
    // Allocate temporary buffer for FAT32 file info
    fat32_file_info_t* fat32_entries = (fat32_file_info_t*)kmalloc(max_entries * sizeof(fat32_file_info_t));
    if (!fat32_entries) {
        error("[FAT32-VFS] Failed to allocate memory for directory listing");
        return VFS_ERROR;
    }
    
    // Resolve path to get directory cluster
    char filename[FAT32_MAX_FILENAME + 1];
    uint32_t dir_cluster = fat32_resolve_path(path, filename);
    if (dir_cluster == 0) {
        error("[FAT32-VFS] Directory not found: %s", path);
        kfree(fat32_entries);
        return VFS_ERROR;
    }
    
    // If there's a filename, this is not a directory path
    if (strlen(filename) > 0) {
        error("[FAT32-VFS] Path is not a directory: %s", path);
        kfree(fat32_entries);
        return VFS_ERROR;
    }
    
    int count = fat32_list_directory(dir_cluster, fat32_entries, max_entries);
    
    if (count < 0) {
        error("[FAT32-VFS] Failed to list FAT32 directory");
        kfree(fat32_entries);
        return VFS_ERROR;
    }
    
    // Convert FAT32 entries to VFS entries
    for (int i = 0; i < count; i++) {
        strncpy(entries[i].name, fat32_entries[i].filename, VFS_MAX_NAME - 1);
        entries[i].name[VFS_MAX_NAME - 1] = '\0';
        entries[i].type = (fat32_entries[i].attributes & FAT32_ATTR_DIRECTORY) ? 
                          VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
        entries[i].size = fat32_entries[i].size;
    }
    
    kfree(fat32_entries);
    success("[FAT32-VFS] Found %d entries in FAT32 directory", count);
    return count;
}

static int fat32_vfs_mkdir(vfs_mount_t* mount, const char* path) {
    (void)mount;
    (void)path;
    
    error("[FAT32-VFS] mkdir not supported (read-only filesystem)");
    return VFS_ERROR;
}

static int fat32_vfs_rmdir(vfs_mount_t* mount, const char* path) {
    (void)mount;
    (void)path;
    
    error("[FAT32-VFS] rmdir not supported (read-only filesystem)");
    return VFS_ERROR;
}

static int fat32_vfs_create(vfs_mount_t* mount, const char* path) {
    (void)mount;
    (void)path;
    
    error("[FAT32-VFS] create not supported (read-only filesystem)");
    return VFS_ERROR;
}

static int fat32_vfs_remove(vfs_mount_t* mount, const char* path) {
    (void)mount;
    (void)path;
    
    error("[FAT32-VFS] remove not supported (read-only filesystem)");
    return VFS_ERROR;
}

static int fat32_vfs_stat(vfs_mount_t* mount, const char* path, vfs_dirent_t* info) {
    (void)mount; // Mount info available if needed
    
    // Handle root directory
    if (strcmp(path, "/") == 0) {
        strcpy(info->name, "/");
        info->type = VFS_TYPE_DIRECTORY;
        info->size = 0;
        return VFS_SUCCESS;
    }
    
    // Resolve path to get directory and filename
    char filename[FAT32_MAX_FILENAME + 1];
    uint32_t dir_cluster = fat32_resolve_path(path, filename);
    if (dir_cluster == 0) {
        return VFS_NOT_FOUND;
    }
    
    // If no filename, this is a directory path
    if (strlen(filename) == 0) {
        // Extract directory name from path
        const char* last_slash = strrchr(path, '/');
        if (last_slash && last_slash != path) {
            strncpy(info->name, last_slash + 1, VFS_MAX_NAME - 1);
            info->name[VFS_MAX_NAME - 1] = '\0';
        } else {
            strcpy(info->name, "/");
        }
        info->type = VFS_TYPE_DIRECTORY;
        info->size = 0;
        return VFS_SUCCESS;
    }
    
    // Find the file in the directory
    fat32_file_info_t fat32_entries[32];
    int count = fat32_list_directory(dir_cluster, fat32_entries, 32);
    
    for (int i = 0; i < count; i++) {
        if (strcmp(fat32_entries[i].filename, filename) == 0) {
            strncpy(info->name, fat32_entries[i].filename, VFS_MAX_NAME - 1);
            info->name[VFS_MAX_NAME - 1] = '\0';
            info->type = (fat32_entries[i].attributes & FAT32_ATTR_DIRECTORY) ? 
                         VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
            info->size = fat32_entries[i].size;
            return VFS_SUCCESS;
        }
    }
    
    return VFS_NOT_FOUND;
}

// Global FAT32 VFS operations structure
static vfs_operations_t fat32_vfs_ops = {
    .open = fat32_vfs_open,
    .read = fat32_vfs_read,
    .write = fat32_vfs_write,
    .seek = fat32_vfs_seek,
    .close = fat32_vfs_close,
    .readdir = fat32_vfs_readdir,
    .mkdir = fat32_vfs_mkdir,
    .rmdir = fat32_vfs_rmdir,
    .create = fat32_vfs_create,
    .remove = fat32_vfs_remove,
    .stat = fat32_vfs_stat
};

// Function to get FAT32 VFS operations
vfs_operations_t* fat32_get_vfs_ops(void) {
    return &fat32_vfs_ops;
}

// Function to mount FAT32 via VFS
int fat32_vfs_mount(const char* mountpoint, uint8_t device_id) {
    debug("[FAT32-VFS] Mounting FAT32 on device %d at %s", device_id, mountpoint);
    
    // First mount FAT32 using existing function
    if (fat32_mount(device_id) != 0) {
        error("[FAT32-VFS] Failed to mount FAT32 on device %d", device_id);
        return VFS_ERROR;
    }
    
    // Then register with VFS
    return vfs_mount(mountpoint, VFS_FS_FAT32, device_id, &fat32_vfs_ops, NULL);
}
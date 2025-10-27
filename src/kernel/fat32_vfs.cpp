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
    
    if (!path || path[0] == '\0' || strcmp(path, "/") == 0) {
        error("[FAT32-VFS] Cannot open root directory as file");
        return VFS_ERROR;
    }

    fat32_file_info_t info;
    if (fat32_lookup_path(path, &info, NULL, NULL, NULL) != 0) {
        error("[FAT32-VFS] Failed to locate file: %s", path);
        return VFS_NOT_FOUND;
    }

    if (info.attributes & FAT32_ATTR_DIRECTORY) {
        error("[FAT32-VFS] '%s' is a directory", path);
        return VFS_ERROR;
    }

    int fat32_fd = fat32_open(path);
    if (fat32_fd < 0) {
        error("[FAT32-VFS] Failed to open file: %s", path);
        return VFS_NOT_FOUND;
    }
    
    // Store FAT32 file descriptor as handle
    file->fs_handle = (uint32_t)fat32_fd;
    file->position = 0;
    file->in_use = 1;
    
    success("[FAT32-VFS] Successfully opened file: %s (fd=%d)", path, fat32_fd);
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
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }

    int fat32_fd = (int)file->fs_handle;
    int bytes_written = fat32_write(fat32_fd, buffer, size);

    if (bytes_written >= 0) {
        file->position += bytes_written;
        debug("[FAT32-VFS] Wrote %d bytes", bytes_written);
        return bytes_written;
    }

    error("[FAT32-VFS] Failed to write to FAT32 file");
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

static int fat32_vfs_unmount(vfs_mount_t* mount) {
    (void)mount;
    return fat32_unmount();
}

static int fat32_vfs_readdir(vfs_mount_t* mount, const char* path, vfs_dirent_t* entries, int max_entries) {
    (void)mount; // Mount info available if needed
    
    debug("[FAT32-VFS] Reading directory: %s", path);
    
    int alloc_slots = (max_entries > 0) ? max_entries : 1;
    // Allocate temporary buffer for FAT32 file info
    fat32_file_info_t* fat32_entries = (fat32_file_info_t*)kmalloc(alloc_slots * sizeof(fat32_file_info_t));
    if (!fat32_entries) {
        error("[FAT32-VFS] Failed to allocate memory for directory listing");
        return VFS_ERROR;
    }

    const char* target_path = (path && path[0] != '\0') ? path : "/";

    uint32_t dir_cluster;
    if (strcmp(target_path, "/") == 0) {
        dir_cluster = fat32_get_root_cluster();
    } else {
        fat32_file_info_t dir_info;
        if (fat32_lookup_path(target_path, &dir_info, NULL, NULL, NULL) != 0) {
            error("[FAT32-VFS] Directory not found: %s", target_path);
            kfree(fat32_entries);
            return VFS_NOT_FOUND;
        }

        if (!(dir_info.attributes & FAT32_ATTR_DIRECTORY)) {
            error("[FAT32-VFS] Path is not a directory: %s", target_path);
            kfree(fat32_entries);
            return VFS_ERROR;
        }

        dir_cluster = dir_info.cluster;
    }
    
    int count = fat32_list_directory(dir_cluster, fat32_entries, alloc_slots);

    if (count < 0) {
        error("[FAT32-VFS] Failed to list FAT32 directory");
        kfree(fat32_entries);
        return VFS_ERROR;
    }

    int out = 0;

    // Don't add . and .. entries - these are handled by the VFS layer and shell navigation
    // Only list actual directory contents

    for (int i = 0; i < count && out < max_entries; i++) {
        strncpy(entries[out].name, fat32_entries[i].filename, VFS_MAX_NAME - 1);
        entries[out].name[VFS_MAX_NAME - 1] = '\0';
        entries[out].type = (fat32_entries[i].attributes & FAT32_ATTR_DIRECTORY) ? 
                          VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
        entries[out].size = fat32_entries[i].size;
        out++;
    }

    kfree(fat32_entries);
    debug("[FAT32-VFS] Found %d entries in FAT32 directory", out);
    return out;
}

static int fat32_vfs_mkdir(vfs_mount_t* mount, const char* path) {
    (void)mount;

    if (fat32_mkdir_path(path) != 0) {
        return VFS_ERROR;
    }
    return VFS_SUCCESS;
}

static int fat32_vfs_rmdir(vfs_mount_t* mount, const char* path) {
    (void)mount;

    if (fat32_rmdir_path(path) != 0) {
        return VFS_ERROR;
    }
    return VFS_SUCCESS;
}

static int fat32_vfs_create(vfs_mount_t* mount, const char* path) {
    (void)mount;

    if (fat32_create(path) != 0) {
        return VFS_ERROR;
    }
    return VFS_SUCCESS;
}

static int fat32_vfs_remove(vfs_mount_t* mount, const char* path) {
    (void)mount;

    if (fat32_remove(path) != 0) {
        return VFS_ERROR;
    }
    return VFS_SUCCESS;
}

static int fat32_vfs_stat(vfs_mount_t* mount, const char* path, vfs_dirent_t* info) {
    (void)mount; // Mount info available if needed

    const char* target_path = (path && path[0] != '\0') ? path : "/";

    fat32_file_info_t entry;
    if (fat32_lookup_path(target_path, &entry, NULL, NULL, NULL) != 0) {
        return VFS_NOT_FOUND;
    }

    if (strcmp(target_path, "/") == 0) {
        strcpy(info->name, "/");
        info->type = VFS_TYPE_DIRECTORY;
        info->size = 0;
        return VFS_SUCCESS;
    }

    strncpy(info->name, entry.filename, VFS_MAX_NAME - 1);
    info->name[VFS_MAX_NAME - 1] = '\0';
    info->type = (entry.attributes & FAT32_ATTR_DIRECTORY) ? VFS_TYPE_DIRECTORY : VFS_TYPE_FILE;
    info->size = entry.size;
    
    return VFS_SUCCESS;
}

// Global FAT32 VFS operations structure
static vfs_operations_t fat32_vfs_ops = {
    .open = fat32_vfs_open,
    .read = fat32_vfs_read,
    .write = fat32_vfs_write,
    .seek = fat32_vfs_seek,
    .close = fat32_vfs_close,
    .unmount = fat32_vfs_unmount,
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

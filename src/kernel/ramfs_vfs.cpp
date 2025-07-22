#include "kernel/vfs.h"
#include "kernel/ramfs.h"
#include "kernel/heap.h"
#include <stdio.h>
#include <string.h>

// RamFS VFS adapter functions

static int ramfs_vfs_open(vfs_mount_t* mount, const char* path, vfs_file_t* file) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Opening file: %s\n", path);
    
    FSNode* node = fs_find_by_path(path);
    if (!node) {
        printf("[RAMFS-VFS] Node not found for path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    printf("[RAMFS-VFS] Found node: %s, type=%d\n", node->name, node->type);
    
    if (node->type != FS_FILE) {
        printf("[RAMFS-VFS] Node is not a file, type=%d\n", node->type);
        return VFS_ERROR;
    }
    
    // For RamFS, we store the FSNode pointer as the handle
    file->fs_handle = (uint32_t)node;
    file->position = 0;
    file->in_use = 1;
    
    return VFS_SUCCESS;
}

static int ramfs_vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }
    
    FSNode* node = (FSNode*)file->fs_handle;
    if (!node || !node->data) {
        return VFS_ERROR;
    }
    
    // Check bounds
    if (file->position >= node->size) {
        return 0; // EOF
    }
    
    // Adjust size if reading past end
    size_t remaining = node->size - file->position;
    if (size > remaining) {
        size = remaining;
    }
    
    // Copy data
    memcpy(buffer, node->data + file->position, size);
    file->position += size;
    
    return size;
}

static int ramfs_vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }
    
    FSNode* node = (FSNode*)file->fs_handle;
    if (!node) {
        return VFS_ERROR;
    }
    
    // Check if we need to expand the file
    size_t new_size = file->position + size;
    if (new_size > node->size) {
        // Reallocate data (handle case where data is initially NULL)
        uint8_t* new_data;
        if (node->data == NULL) {
            new_data = (uint8_t*)kmalloc(new_size);
        } else {
            new_data = (uint8_t*)krealloc(node->data, new_size);
        }
        
        if (!new_data) {
            printf("[RAMFS-VFS] Failed to allocate memory for write\n");
            return VFS_ERROR;
        }
        node->data = new_data;
        node->size = new_size;
    }
    
    // Copy data
    memcpy(node->data + file->position, buffer, size);
    file->position += size;
    
    printf("[RAMFS-VFS] Wrote %u bytes at position %u\n", size, file->position - size);
    return size;
}

static int ramfs_vfs_seek(vfs_file_t* file, uint32_t position) {
    if (!file || !file->in_use) {
        return VFS_ERROR;
    }
    
    FSNode* node = (FSNode*)file->fs_handle;
    if (!node) {
        return VFS_ERROR;
    }
    
    // Allow seeking beyond file size (for writing)
    file->position = position;
    printf("[RAMFS-VFS] Seeked to position %u\n", position);
    return VFS_SUCCESS;
}

static void ramfs_vfs_close(vfs_file_t* file) {
    if (file && file->in_use) {
        printf("[RAMFS-VFS] Closing file\n");
        file->in_use = 0;
        file->fs_handle = 0;
        file->position = 0;
    }
}

static int ramfs_vfs_readdir(vfs_mount_t* mount, const char* path, vfs_dirent_t* entries, int max_entries) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Reading directory: %s\n", path);
    
    FSNode* dir = fs_find_by_path(path);
    if (!dir) {
        printf("[RAMFS-VFS] Directory not found: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (dir->type != FS_DIRECTORY) {
        printf("[RAMFS-VFS] Path is not a directory: %s\n", path);
        return VFS_ERROR;
    }
    
    int count = 0;
    size_t max_count = (size_t)max_entries;
    
    // Don't add . and .. entries - these are handled by the VFS layer and shell navigation
    // Only list actual directory contents
    
    // RamFS uses an array of children pointers
    for (size_t i = 0; i < dir->child_count && count < max_count; i++) {
        FSNode* child = dir->children[i];
        if (child) {
            strncpy(entries[count].name, child->name, VFS_MAX_NAME - 1);
            entries[count].name[VFS_MAX_NAME - 1] = '\0';
            entries[count].type = (child->type == FS_FILE) ? VFS_TYPE_FILE : VFS_TYPE_DIRECTORY;
            entries[count].size = child->size;
            count++;
        }
    }
    
    printf("[RAMFS-VFS] Found %d entries in directory\n", count);
    return count;
}

static int ramfs_vfs_mkdir(vfs_mount_t* mount, const char* path) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Creating directory: %s\n", path);
    
    FSNode* node = fs_mkdir(path);
    if (!node) {
        printf("[RAMFS-VFS] Failed to create directory: %s\n", path);
        return VFS_ERROR;
    }
    
    printf("[RAMFS-VFS] Successfully created directory: %s\n", path);
    return VFS_SUCCESS;
}

static int ramfs_vfs_rmdir(vfs_mount_t* mount, const char* path) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Removing directory: %s\n", path);
    
    int result = fs_rmdir(path);
    if (result == 0) {
        printf("[RAMFS-VFS] Successfully removed directory: %s\n", path);
        return VFS_SUCCESS;
    } else {
        printf("[RAMFS-VFS] Failed to remove directory: %s\n", path);
        return VFS_ERROR;
    }
}

static int ramfs_vfs_create(vfs_mount_t* mount, const char* path) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Creating file: %s\n", path);
    
    // Use the existing fs_touch function which should work correctly
    FSNode* node = fs_touch(path);
    if (!node) {
        printf("[RAMFS-VFS] fs_touch returned NULL for: %s\n", path);
        return VFS_ERROR;
    }
    
    printf("[RAMFS-VFS] fs_touch created node with type=%d (should be %d)\n", node->type, FS_FILE);
    
    return VFS_SUCCESS;
}

static int ramfs_vfs_remove(vfs_mount_t* mount, const char* path) {
    (void)mount; // Unused for RamFS
    
    printf("[RAMFS-VFS] Removing file: %s\n", path);
    
    int result = fs_remove(path);
    if (result == 0) {
        printf("[RAMFS-VFS] Successfully removed file: %s\n", path);
        return VFS_SUCCESS;
    } else {
        printf("[RAMFS-VFS] Failed to remove file: %s\n", path);
        return VFS_ERROR;
    }
}

static int ramfs_vfs_stat(vfs_mount_t* mount, const char* path, vfs_dirent_t* info) {
    (void)mount; // Unused for RamFS
    
    FSNode* node = fs_find_by_path(path);
    if (!node) {
        return VFS_NOT_FOUND;
    }
    
    strncpy(info->name, node->name, VFS_MAX_NAME - 1);
    info->name[VFS_MAX_NAME - 1] = '\0';
    info->type = (node->type == FS_FILE) ? VFS_TYPE_FILE : VFS_TYPE_DIRECTORY;
    info->size = node->size;
    
    return VFS_SUCCESS;
}

// Global RamFS VFS operations structure
static vfs_operations_t ramfs_vfs_ops = {
    .open = ramfs_vfs_open,
    .read = ramfs_vfs_read,
    .write = ramfs_vfs_write,
    .seek = ramfs_vfs_seek,
    .close = ramfs_vfs_close,
    .readdir = ramfs_vfs_readdir,
    .mkdir = ramfs_vfs_mkdir,
    .rmdir = ramfs_vfs_rmdir,
    .create = ramfs_vfs_create,
    .remove = ramfs_vfs_remove,
    .stat = ramfs_vfs_stat
};

// Function to get RamFS VFS operations
vfs_operations_t* ramfs_get_vfs_ops(void) {
    return &ramfs_vfs_ops;
}

// Function to mount RamFS via VFS
int ramfs_vfs_mount(const char* mountpoint) {
    printf("[RAMFS-VFS] Mounting RamFS at %s\n", mountpoint);
    return vfs_mount(mountpoint, VFS_FS_RAMFS, 0, &ramfs_vfs_ops, NULL);
}
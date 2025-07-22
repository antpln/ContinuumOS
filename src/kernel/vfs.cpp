#include "kernel/vfs.h"
#include "kernel/fat32.h"
#include <stdio.h>
#include <string.h>

static vfs_mount_t mounts[VFS_MAX_MOUNTS];
static vfs_file_t open_files[VFS_MAX_OPEN_FILES];
static char current_working_directory[VFS_MAX_PATH];
static uint8_t vfs_initialized = 0;

int vfs_init(void) {
    printf("[VFS] Initializing Virtual File System\n");
    
    // Clear mount table
    memset(mounts, 0, sizeof(mounts));
    
    // Clear open file table
    memset(open_files, 0, sizeof(open_files));
    
    // Set initial working directory to root
    strcpy(current_working_directory, "/");
    
    vfs_initialized = 1;
    printf("[VFS] VFS initialized successfully\n");
    return VFS_SUCCESS;
}

int vfs_mount(const char* mountpoint, uint8_t fs_type, uint8_t device_id, 
              vfs_operations_t* ops, void* fs_data) {
    if (!vfs_initialized) {
        printf("[VFS] VFS not initialized\n");
        return VFS_ERROR;
    }
    
    if (!mountpoint || !ops) {
        printf("[VFS] Invalid mount parameters\n");
        return VFS_ERROR;
    }
    
    printf("[VFS] Mounting filesystem type %d at %s\n", fs_type, mountpoint);
    
    // Validate filesystem type
    if (fs_type != VFS_FS_RAMFS && fs_type != VFS_FS_FAT32) {
        printf("[VFS] Unsupported filesystem type: %d\n", fs_type);
        return VFS_ERROR;
    }
    
    // Check if mountpoint already exists
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].mounted && strcmp(mounts[i].mountpoint, mountpoint) == 0) {
            printf("[VFS] Mountpoint %s already mounted\n", mountpoint);
            return VFS_ALREADY_MOUNTED;
        }
    }
    
    // Find free mount slot
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (!mounts[i].mounted) {
            strcpy(mounts[i].mountpoint, mountpoint);
            mounts[i].fs_type = fs_type;
            mounts[i].device_id = device_id;
            mounts[i].ops = ops;
            mounts[i].fs_data = fs_data;
            mounts[i].mounted = 1;
            
            printf("[VFS] Successfully mounted filesystem at %s\n", mountpoint);
            return VFS_SUCCESS;
        }
    }
    
    printf("[VFS] No free mount slots\n");
    return VFS_NO_SPACE;
}

int vfs_unmount(const char* mountpoint) {
    if (!vfs_initialized) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Unmounting %s\n", mountpoint);
    
    // Find mount point
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].mounted && strcmp(mounts[i].mountpoint, mountpoint) == 0) {
            // Close any open files from this mount
            for (int j = 0; j < VFS_MAX_OPEN_FILES; j++) {
                if (open_files[j].in_use && open_files[j].mount == &mounts[i]) {
                    vfs_close(&open_files[j]);
                }
            }
            
            // Call filesystem-specific unmount for FAT32
            if (mounts[i].fs_type == VFS_FS_FAT32) {
                int result = fat32_unmount();
                if (result != 0) {
                    printf("[VFS] Warning: FAT32 unmount returned error %d\n", result);
                }
            }
            
            mounts[i].mounted = 0;
            printf("[VFS] Successfully unmounted %s\n", mountpoint);
            return VFS_SUCCESS;
        }
    }
    
    printf("[VFS] Mountpoint %s not found\n", mountpoint);
    return VFS_NOT_FOUND;
}

vfs_mount_t* vfs_find_mount(const char* path) {
    if (!vfs_initialized) {
        return NULL;
    }
    
    vfs_mount_t* best_mount = NULL;
    size_t best_match_len = 0;
    
    // Find longest matching mount point
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].mounted) {
            size_t mount_len = strlen(mounts[i].mountpoint);
            
            // Check if path starts with this mountpoint
            if (strncmp(path, mounts[i].mountpoint, mount_len) == 0) {
                // Ensure it's a proper path boundary
                if (path[mount_len] == '\0' || path[mount_len] == '/' || 
                    strcmp(mounts[i].mountpoint, "/") == 0) {
                    if (mount_len > best_match_len) {
                        best_match_len = mount_len;
                        best_mount = &mounts[i];
                    }
                }
            }
        }
    }
    
    return best_mount;
}

int vfs_resolve_path(const char* path, vfs_mount_t** mount, char* relative_path) {
    if (!path || !mount || !relative_path) {
        return VFS_ERROR;
    }
    
    // Normalize the path (handles relative paths, . and .. components)
    char absolute_path[VFS_MAX_PATH];
    if (vfs_normalize_path(path, absolute_path) != VFS_SUCCESS) {
        return VFS_ERROR;
    }
    
    // Find the mount point
    *mount = vfs_find_mount(absolute_path);
    if (!*mount) {
        return VFS_NOT_MOUNTED;
    }
    
    // Calculate relative path within the filesystem
    size_t mount_len = strlen((*mount)->mountpoint);
    if (strcmp((*mount)->mountpoint, "/") == 0) {
        // Root mount - use path as-is
        strcpy(relative_path, absolute_path);
    } else {
        // Non-root mount - strip mountpoint prefix
        if (absolute_path[mount_len] == '/') {
            strcpy(relative_path, &absolute_path[mount_len]);
        } else {
            strcpy(relative_path, "/");
        }
    }
    
    return VFS_SUCCESS;
}

int vfs_list_mounts(void) {
    if (!vfs_initialized) {
        printf("[VFS] VFS not initialized\n");
        return 0;
    }
    
    printf("[VFS] Current mount points:\n");
    int count = 0;
    for (int i = 0; i < VFS_MAX_MOUNTS; i++) {
        if (mounts[i].mounted) {
            printf("  %s (type %d, device %d)\n", 
                   mounts[i].mountpoint, mounts[i].fs_type, mounts[i].device_id);
            count++;
        }
    }
    
    if (count == 0) {
        printf("  No filesystems mounted\n");
    }
    
    return count;
}

int vfs_chdir(const char* path) {
    if (!vfs_initialized) {
        return VFS_ERROR;
    }
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        return VFS_NOT_FOUND;
    }
    
    // Build the absolute path properly
    char absolute_path[VFS_MAX_PATH];
    if (path[0] == '/') {
        // Absolute path - use as-is
        strcpy(absolute_path, path);
    } else {
        // Relative path - build from current directory
        strcpy(absolute_path, current_working_directory);
        if (strcmp(current_working_directory, "/") != 0) {
            strcat(absolute_path, "/");
        }
        strcat(absolute_path, path);
    }
    
    // Normalize the path to resolve . and .. components
    char normalized_path[VFS_MAX_PATH];
    if (vfs_normalize_path(absolute_path, normalized_path) != VFS_SUCCESS) {
        return VFS_ERROR;
    }
    
    // Update current working directory with the normalized path
    strcpy(current_working_directory, normalized_path);
    
    printf("[VFS] Changed directory to %s\n", current_working_directory);
    return VFS_SUCCESS;
}

const char* vfs_getcwd(void) {
    return current_working_directory;
}

// VFS file operations with path resolution
int vfs_open(const char* path, vfs_file_t* file) {
    if (!vfs_initialized || !path || !file) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Opening file: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    printf("[VFS] Path resolved: %s -> mount=%p, relative_path='%s'\n", path, mount, relative_path);
    
    if (!mount->ops || !mount->ops->open) {
        printf("[VFS] Filesystem does not support open operation\n");
        return VFS_ERROR;
    }
    
    // Find a free VFS file handle
    vfs_file_t* vfs_handle = NULL;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (!open_files[i].in_use) {
            vfs_handle = &open_files[i];
            break;
        }
    }
    
    if (!vfs_handle) {
        printf("[VFS] No free file handles available\n");
        return VFS_NO_SPACE;
    }
    
    // Call filesystem-specific open
    vfs_handle->mount = mount;
    int result = mount->ops->open(mount, relative_path, vfs_handle);
    
    if (result == VFS_SUCCESS) {
        // Copy the VFS handle to user's file structure
        *file = *vfs_handle;
        printf("[VFS] Successfully opened file: %s\n", path);
    } else {
        // Clear the VFS handle on failure
        vfs_handle->in_use = 0;
        vfs_handle->mount = NULL;
        printf("[VFS] Failed to open file: %s\n", path);
    }
    
    return result;
}

int vfs_read(vfs_file_t* file, void* buffer, size_t size) {
    if (!file || !file->in_use || !file->mount || !file->mount->ops || !file->mount->ops->read) {
        return VFS_ERROR;
    }
    
    return file->mount->ops->read(file, buffer, size);
}

int vfs_write(vfs_file_t* file, const void* buffer, size_t size) {
    if (!file || !file->in_use || !file->mount || !file->mount->ops || !file->mount->ops->write) {
        return VFS_ERROR;
    }
    
    return file->mount->ops->write(file, buffer, size);
}

int vfs_seek(vfs_file_t* file, uint32_t position) {
    if (!file || !file->in_use || !file->mount || !file->mount->ops || !file->mount->ops->seek) {
        return VFS_ERROR;
    }
    
    return file->mount->ops->seek(file, position);
}

void vfs_close(vfs_file_t* file) {
    if (!file || !file->in_use) {
        return;
    }
    
    // Find the corresponding VFS handle in global table
    vfs_file_t* vfs_handle = NULL;
    for (int i = 0; i < VFS_MAX_OPEN_FILES; i++) {
        if (open_files[i].in_use && 
            open_files[i].fs_handle == file->fs_handle && 
            open_files[i].mount == file->mount) {
            vfs_handle = &open_files[i];
            break;
        }
    }
    
    // Call filesystem-specific close
    if (file->mount && file->mount->ops && file->mount->ops->close) {
        file->mount->ops->close(file);
    }
    
    // Clear both the user's file structure and VFS handle
    if (vfs_handle) {
        vfs_handle->in_use = 0;
        vfs_handle->mount = NULL;
        vfs_handle->fs_handle = 0;
        vfs_handle->position = 0;
    }
    
    file->in_use = 0;
    file->mount = NULL;
    file->fs_handle = 0;
    file->position = 0;
}

int vfs_readdir(const char* path, vfs_dirent_t* entries, int max_entries) {
    if (!vfs_initialized || !path || !entries) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Reading directory: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->readdir) {
        printf("[VFS] Filesystem does not support readdir operation\n");
        return VFS_ERROR;
    }
    
    return mount->ops->readdir(mount, relative_path, entries, max_entries);
}

int vfs_mkdir(const char* path) {
    if (!vfs_initialized || !path) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Creating directory: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->mkdir) {
        printf("[VFS] Filesystem does not support mkdir operation\n");
        return VFS_ERROR;
    }
    
    return mount->ops->mkdir(mount, relative_path);
}

int vfs_rmdir(const char* path) {
    if (!vfs_initialized || !path) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Removing directory: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->rmdir) {
        printf("[VFS] Filesystem does not support rmdir operation\n");
        return VFS_ERROR;
    }
    
    return mount->ops->rmdir(mount, relative_path);
}

int vfs_create(const char* path) {
    if (!vfs_initialized || !path) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Creating file: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->create) {
        printf("[VFS] Filesystem does not support create operation\n");
        return VFS_ERROR;
    }
    
    return mount->ops->create(mount, relative_path);
}

int vfs_remove(const char* path) {
    if (!vfs_initialized || !path) {
        return VFS_ERROR;
    }
    
    printf("[VFS] Removing file: %s\n", path);
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        printf("[VFS] Failed to resolve path: %s\n", path);
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->remove) {
        printf("[VFS] Filesystem does not support remove operation\n");
        return VFS_ERROR;
    }
    
    return mount->ops->remove(mount, relative_path);
}

int vfs_stat(const char* path, vfs_dirent_t* info) {
    if (!vfs_initialized || !path || !info) {
        return VFS_ERROR;
    }
    
    vfs_mount_t* mount;
    char relative_path[VFS_MAX_PATH];
    
    if (vfs_resolve_path(path, &mount, relative_path) != VFS_SUCCESS) {
        return VFS_NOT_FOUND;
    }
    
    if (!mount->ops || !mount->ops->stat) {
        return VFS_ERROR;
    }
    
    return mount->ops->stat(mount, relative_path, info);
}

// Normalize a path by removing multiple slashes and resolving . and .. components
int vfs_normalize_path(const char* path, char* normalized_path) {
    if (!path || !normalized_path) {
        return VFS_ERROR;
    }
    
    // Handle simple cases first
    if (strcmp(path, ".") == 0) {
        strcpy(normalized_path, current_working_directory);
        return VFS_SUCCESS;
    }
    if (strcmp(path, "..") == 0) {
        // Go to parent of current directory
        if (strcmp(current_working_directory, "/") == 0) {
            strcpy(normalized_path, "/");
        } else {
            // Find the last slash in current directory
            char temp[VFS_MAX_PATH];
            strcpy(temp, current_working_directory);
            char* last_slash = strrchr(temp, '/');
            if (last_slash && last_slash != temp) {
                *last_slash = '\0';
                strcpy(normalized_path, temp);
            } else {
                strcpy(normalized_path, "/");
            }
        }
        return VFS_SUCCESS;
    }
    
    // First resolve . and .. components
    char temp_path[VFS_MAX_PATH];
    if (vfs_resolve_dots(path, temp_path) != VFS_SUCCESS) {
        return VFS_ERROR;
    }
    
    // Now clean up multiple slashes
    char* src = temp_path;
    char* dst = normalized_path;
    int last_was_slash = 0;
    
    while (*src && (dst - normalized_path) < VFS_MAX_PATH - 1) {
        if (*src == '/') {
            if (!last_was_slash) {
                *dst++ = '/';
                last_was_slash = 1;
            }
        } else {
            *dst++ = *src;
            last_was_slash = 0;
        }
        src++;
    }
    
    *dst = '\0';
    
    // Remove trailing slash (except for root)
    if (dst > normalized_path + 1 && *(dst - 1) == '/') {
        *(dst - 1) = '\0';
    }
    
    // Ensure root is "/" not empty
    if (strlen(normalized_path) == 0) {
        strcpy(normalized_path, "/");
    }
    
    return VFS_SUCCESS;
}

// Resolve . and .. path components
int vfs_resolve_dots(const char* path, char* resolved_path) {
    if (!path || !resolved_path) {
        return VFS_ERROR;
    }
    
    // Handle absolute vs relative paths
    char working_path[VFS_MAX_PATH];
    if (path[0] == '/') {
        strcpy(working_path, path);
    } else {
        // Relative path - prepend current working directory
        strcpy(working_path, current_working_directory);
        if (strcmp(current_working_directory, "/") != 0) {
            strcat(working_path, "/");
        }
        strcat(working_path, path);
    }
    
    // Split path into components and process them
    char components[32][VFS_MAX_NAME];  // Max 32 path components
    int component_count = 0;
    
    char* token = strtok(working_path, "/");
    while (token && component_count < 32) {
        if (strcmp(token, ".") == 0) {
            // Current directory - skip
            continue;
        } else if (strcmp(token, "..") == 0) {
            // Parent directory - go back one level
            if (component_count > 0) {
                component_count--;
            }
            // If we're at root, .. has no effect
        } else {
            // Regular component
            strcpy(components[component_count], token);
            component_count++;
        }
        token = strtok(NULL, "/");
    }
    
    // Rebuild the path
    if (component_count == 0) {
        strcpy(resolved_path, "/");
    } else {
        resolved_path[0] = '\0';
        for (int i = 0; i < component_count; i++) {
            strcat(resolved_path, "/");
            strcat(resolved_path, components[i]);
        }
    }
    
    return VFS_SUCCESS;
}
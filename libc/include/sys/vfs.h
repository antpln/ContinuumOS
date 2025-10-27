#ifndef LIBC_SYS_VFS_H
#define LIBC_SYS_VFS_H

#include <kernel/vfs.h>
#include <sys/syscall.h>

static inline int vfs_user_open(const char* path, vfs_file_t* file) {
    return syscall_vfs_open(path, file);
}

static inline int vfs_user_read(vfs_file_t* file, void* buffer, size_t size) {
    return syscall_vfs_read(file, buffer, size);
}

static inline int vfs_user_write(vfs_file_t* file, const void* buffer, size_t size) {
    return syscall_vfs_write(file, buffer, size);
}

static inline int vfs_user_seek(vfs_file_t* file, uint32_t position) {
    return syscall_vfs_seek(file, position);
}

static inline void vfs_user_close(vfs_file_t* file) {
    syscall_vfs_close(file);
}

static inline int vfs_user_create(const char* path) {
    return syscall_vfs_create(path);
}

static inline int vfs_user_remove(const char* path) {
    return syscall_vfs_remove(path);
}

static inline int vfs_user_stat(const char* path, vfs_dirent_t* info) {
    return syscall_vfs_stat(path, info);
}

static inline int vfs_user_mkdir(const char* path) {
    return syscall_vfs_mkdir(path);
}

static inline int vfs_user_rmdir(const char* path) {
    return syscall_vfs_rmdir(path);
}

static inline int vfs_user_readdir(const char* path, vfs_dirent_t* entries, int max_entries) {
    return syscall_vfs_readdir(path, entries, max_entries);
}

static inline int vfs_user_normalize_path(const char* path, char* normalized_path) {
    return syscall_vfs_normalize_path(path, normalized_path);
}

#endif // LIBC_SYS_VFS_H

#ifndef RAMFS_H
#define RAMFS_H

#define MAX_OPEN_FILES 64
// Maximum number of children a directory can have.
#define MAX_CHILDREN 32

#include <stddef.h>
#include <stdint.h>

// Types of filesystem nodes.
enum FSNodeType {
    FS_FILE,
    FS_DIRECTORY,
};



// A filesystem node: either a file or a directory.
typedef struct FSNode {
    char name[64];
    FSNodeType type;
    size_t size;         // For files: current size; for directories: may be unused.
    uint8_t* data;       // For files: pointer to file contents (allocated via kmalloc).
    struct FSNode* parent;
    struct FSNode** children; // Array of pointers to children (if directory)
    size_t child_count;
} FSNode;

typedef struct {
    FSNode* node;      // Pointer to file node
    size_t offset;     // Current read/write offset
    int used;          // 1 if occupied, 0 otherwise
} FileDescriptor;

extern FileDescriptor fd_table[MAX_OPEN_FILES];


// Filesystem interface.
FSNode* fs_create_node(const char* name, FSNodeType type);
void fs_add_child(FSNode* parent, FSNode* child);
void fs_remove_child(FSNode* parent, FSNode* child);
FSNode* fs_find_child(FSNode* parent, const char* name);

void fs_init();
FSNode* fs_get_root();

// Basic file operations.
int fs_read(FSNode* file, size_t offset, size_t size, uint8_t* buffer);
int fs_write(FSNode* file, size_t offset, size_t size, const uint8_t* buffer);
int fs_open(FSNode* file);
void fs_close(int fd);
FSNode* fs_find_by_path(const char* path);
FSNode* fs_find_by_path_from(const char* path, FSNode* current);
FSNode* fs_mkdir(const char* path);
FSNode* fs_touch(const char* path);
int fs_remove(const char* path);
int fs_rmdir(const char* path);
void fs_free_node(FSNode* node);

#endif // RAMFS_H

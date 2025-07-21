#include "kernel/ramfs.h"
#include "stdio.h"
#include "string.h"
#include "kernel/heap.h"
#include "string.h"
#include <kernel/debug.h>

// For dynamic allocation, we assume a kernel allocator is available.

static FSNode *root = NULL;
FileDescriptor fd_table[MAX_OPEN_FILES];

FSNode *fs_create_node(const char *name, FSNodeType type)
{
    FSNode *node = (FSNode *)kmalloc(sizeof(FSNode));
    if (!node)
        return NULL;
    memset(node, 0, sizeof(FSNode));
    // Copy the name (ensure null termination)
    for (size_t i = 0; i < sizeof(node->name) - 1 && name[i]; i++)
    {
        node->name[i] = name[i];
    }
    node->type = type;
    node->size = 0;
    node->data = NULL;
    node->parent = NULL;
    node->child_count = 0;
    if (type == FS_DIRECTORY)
    {
        // Allocate space for children pointers.
        node->children = (FSNode **)kmalloc(sizeof(FSNode *) * MAX_CHILDREN);
        if (node->children)
        {
            memset(node->children, 0, sizeof(FSNode *) * MAX_CHILDREN);
        }
    }
    return node;
}

void fs_add_child(FSNode *parent, FSNode *child)
{
    if (!parent || parent->type != FS_DIRECTORY)
        return;
    if (parent->child_count >= MAX_CHILDREN)
    {
        error("[RAMFS] directory '%s' is full.", parent->name);
        return;
    }
    parent->children[parent->child_count++] = child;
    child->parent = parent;
}

void fs_remove_child(FSNode *parent, FSNode *child)
{
    if (!parent || parent->type != FS_DIRECTORY)
        return;
    for (size_t i = 0; i < parent->child_count; i++)
    {
        if (parent->children[i] == child)
        {
            // If the child has children, recursively remove them.
            if (child->type == FS_DIRECTORY)
            {
                for (size_t j = 0; j < child->child_count; j++)
                {
                    fs_remove_child(child, child->children[j]);
                }
            }
            // Shift remaining children to the left.
            for (size_t j = i; j < parent->child_count - 1; j++)
            {
                parent->children[j] = parent->children[j + 1];
            }
            parent->child_count--;
            return;
        }
    }
}

FSNode *fs_find_child(FSNode *parent, const char *name)
{
    if (!parent || parent->type != FS_DIRECTORY)
        return NULL;
    for (size_t i = 0; i < parent->child_count; i++)
    {
        if (strcmp(parent->children[i]->name, name) == 0)
        {
            return parent->children[i];
        }
    }
    return NULL;
}

void fs_init()
{
    // Create the root directory.
    root = fs_create_node("/", FS_DIRECTORY);
    if (root == NULL)
    {
        error("[RAMFS] Error initializing filesystem: could not allocate root directory.");
    }
    else
    {
        success("[RAMFS] Filesystem initialized.");
    }
}

FSNode *fs_get_root()
{
    return root;
}

int fs_read(FSNode *file, size_t offset, size_t size, uint8_t *buffer)
{
    if (!file || file->type != FS_FILE)
    {
        return -1;
    }
    if (offset > file->size)
    {
        return -1;
    }
    size_t read_size = size;
    if (offset + size > file->size)
    {
        read_size = file->size - offset;
    }
    memcpy(buffer, file->data + offset, read_size);
    return (int)read_size;
}

int fs_write(FSNode *file, size_t offset, size_t size, const uint8_t *buffer)
{
    if (!file || file->type != FS_FILE)
    {
        return -1;
    }

    // If offset + size exceeds current allocation, resize.
    if (offset + size > file->size)
    {
        uint8_t *new_data = (uint8_t *)krealloc(file->data, offset + size);
        if (!new_data)
        {
            printf("Write error: Out of memory for '%s'", file->name);
            return -1;
        }
        file->data = new_data;
        file->size = offset + size;
    }

    // Perform the write operation
    memcpy(file->data + offset, buffer, size);
    return (int)size;
}

int fs_open(FSNode *node)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
        if (!fd_table[i].used)
        {
            fd_table[i].node = node;
            fd_table[i].offset = 0;
            fd_table[i].used = 1;
            return i; // File descriptor number
        }
    }
    return -1; // No available file descriptor
}

void fs_close(int fd)
{
    if (fd >= 0 && fd < MAX_OPEN_FILES && fd_table[fd].used)
    {
        fd_table[fd].used = 0;
    }
}

FSNode *fs_find_by_path(const char *path)
{
    if (!path || path[0] != '/')
        return NULL; // Ensure it's an absolute path

    FSNode *current = fs_get_root();
    char temp[128];
    size_t i = 0;

    while (*path)
    {
        if (*path == '/' || *path == '\0')
        {
            temp[i] = '\0'; // End of component
            if (strlen(temp) > 0)
            {
                current = fs_find_child(current, temp);
                if (!current)
                    return NULL; // Path not found
            }
            i = 0;
        }
        else
        {
            temp[i++] = *path;
        }
        path++;
    }
    return current;
}

FSNode *fs_find_by_path(const char *path, FSNode *current)
{
    if (!path)
        return NULL;
    char temp[128];
    size_t i = 0;

    // If the path starts with '/', start from the root
    if (path[0] == '/')
    {
        current = fs_get_root();
        path++;
    }

    while (*path)
    {
        if (*path == '/' || *path == '\0')
        {
            temp[i] = '\0'; // End of component
            if (strlen(temp) > 0)
            {
                current = fs_find_child(current, temp);
                if (!current)
                    return NULL; // Path not found
            }
            i = 0;
        }
        else
        {
            temp[i++] = *path;
        }
        path++;
    }
    temp[i] = '\0'; // End of component
    if (strlen(temp) > 0)
    {
        current = fs_find_child(current, temp);
        if (!current)
            return NULL; // Path not found
    }
    return current;
}
// Splits a path into parent directory and name
void split_path(const char *path, char *parent_path, char *name)
{
    const char *last_slash = strrchr(path, '/');
    if (last_slash)
    {
        size_t parent_len = last_slash - path;
        strncpy(parent_path, path, parent_len);
        parent_path[parent_len] = '\0';
        strcpy(name, last_slash + 1);
    }
    else
    {
        strcpy(parent_path, "");
        strcpy(name, path);
    }
}

FSNode *fs_mkdir(const char *path)
{
    char parent_path[128], name[64];
    split_path(path, parent_path, name);

    FSNode *parent = fs_find_by_path(parent_path);
    if (!parent || parent->type != FS_DIRECTORY)
        return NULL;

    FSNode *new_dir = fs_create_node(name, FS_DIRECTORY);
    fs_add_child(parent, new_dir);
    return new_dir;
}
FSNode *fs_touch(const char *path)
{
    char parent_path[128], name[64];
    split_path(path, parent_path, name);

    FSNode *parent = fs_find_by_path(parent_path);
    if (!parent || parent->type != FS_DIRECTORY)
        return NULL;

    FSNode *new_file = fs_create_node(name, FS_FILE);
    new_file->data = (uint8_t *)kmalloc(1024); // Allocate 1KiB buffer
    new_file->size = 0;
    fs_add_child(parent, new_file);
    return new_file;
}

#include "kernel/ramfs.h"
#include "kernel/vfs.h"
#include "stdio.h"
#include "string.h"
#include "kernel/heap.h"

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
        printf("Error: directory '%s' is full.\n", parent->name);
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
        printf("Error initializing filesystem: could not allocate root directory.\n");
    }
    else
    {
        printf("[RAMFS] Filesystem initialized.\n");
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
            printf("Write error: Out of memory for '%s'\n", file->name);
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

    printf("[FS_FIND] Looking for path: %s\n", path);
    
    FSNode *current = fs_get_root();
    char temp[VFS_MAX_NAME];
    size_t i = 0;

    // Handle root directory case
    if (strcmp(path, "/") == 0) {
        printf("[FS_FIND] Returning root directory\n");
        return current;
    }
    
    // Skip initial slash
    path++;

    while (*path)
    {
        if (*path == '/' || *path == '\0')
        {
            temp[i] = '\0'; // End of component
            if (strlen(temp) > 0)
            {
                printf("[FS_FIND] Looking for child: '%s' in directory '%s'\n", temp, current->name);
                
                // Handle special directory entries
                if (strcmp(temp, ".") == 0) {
                    // Current directory - no change needed
                    printf("[FS_FIND] Staying in current directory: '%s'\n", current->name);
                } else if (strcmp(temp, "..") == 0) {
                    // Parent directory
                    if (current->parent) {
                        current = current->parent;
                        printf("[FS_FIND] Moved to parent directory: '%s'\n", current->name);
                    } else {
                        printf("[FS_FIND] Already at root, .. has no effect\n");
                    }
                } else {
                    // Regular child lookup
                    current = fs_find_child(current, temp);
                    if (!current) {
                        printf("[FS_FIND] Child '%s' not found\n", temp);
                        return NULL; // Path not found
                    }
                    printf("[FS_FIND] Found child: '%s', type=%d\n", current->name, current->type);
                }
            }
            i = 0;
        }
        else
        {
            if (i < VFS_MAX_NAME - 1) { // Prevent buffer overflow
                temp[i++] = *path;
            } else {
                return NULL; // Component name too long
            }
        }
        path++;
    }
    
    // Handle last component if path doesn't end with '/'
    temp[i] = '\0';
    if (strlen(temp) > 0) {
        printf("[FS_FIND] Looking for final child: '%s' in directory '%s'\n", temp, current->name);
        
        // Handle special directory entries
        if (strcmp(temp, ".") == 0) {
            // Current directory - no change needed
            printf("[FS_FIND] Final component is current directory: '%s'\n", current->name);
        } else if (strcmp(temp, "..") == 0) {
            // Parent directory
            if (current->parent) {
                current = current->parent;
                printf("[FS_FIND] Final component moved to parent directory: '%s'\n", current->name);
            } else {
                printf("[FS_FIND] Final component: already at root, .. has no effect\n");
            }
        } else {
            // Regular child lookup
            current = fs_find_child(current, temp);
            if (!current) {
                printf("[FS_FIND] Final child '%s' not found\n", temp);
                return NULL;
            }
            printf("[FS_FIND] Found final child: '%s', type=%d\n", current->name, current->type);
        }
    }
    
    return current;
}

FSNode *fs_find_by_path_from(const char *path, FSNode *current)
{
    if (!path)
        return NULL;
    char temp[VFS_MAX_NAME];
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
                // Handle special directory entries
                if (strcmp(temp, ".") == 0) {
                    // Current directory - no change needed
                } else if (strcmp(temp, "..") == 0) {
                    // Parent directory
                    if (current->parent) {
                        current = current->parent;
                    }
                    // If no parent, stay at current (root)
                } else {
                    current = fs_find_child(current, temp);
                    if (!current)
                        return NULL; // Path not found
                }
            }
            i = 0;
        }
        else
        {
            if (i < VFS_MAX_NAME - 1) { // Prevent buffer overflow
                temp[i++] = *path;
            } else {
                return NULL; // Component name too long
            }
        }
        path++;
    }
    temp[i] = '\0'; // End of component
    if (strlen(temp) > 0)
    {
        // Handle special directory entries
        if (strcmp(temp, ".") == 0) {
            // Current directory - no change needed
        } else if (strcmp(temp, "..") == 0) {
            // Parent directory
            if (current->parent) {
                current = current->parent;
            }
            // If no parent, stay at current (root)
        } else {
            current = fs_find_child(current, temp);
            if (!current)
                return NULL; // Path not found
        }
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
        if (parent_len == 0) {
            // Root directory case: /filename -> parent="/", name="filename"
            strcpy(parent_path, "/");
        } else {
            strncpy(parent_path, path, parent_len);
            parent_path[parent_len] = '\0';
        }
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

    printf("[FS_TOUCH] Path: '%s' -> parent: '%s', name: '%s'\n", path, parent_path, name);

    FSNode *parent = fs_find_by_path(parent_path);
    if (!parent) {
        printf("[FS_TOUCH] Parent not found: '%s'\n", parent_path);
        return NULL;
    }
    if (parent->type != FS_DIRECTORY) {
        printf("[FS_TOUCH] Parent is not a directory: '%s' (type=%d)\n", parent_path, parent->type);
        return NULL;
    }

    printf("[FS_TOUCH] Creating file node with FS_FILE=%d\n", FS_FILE);
    FSNode *new_file = fs_create_node(name, FS_FILE);
    if (!new_file) {
        printf("[FS_TOUCH] Failed to create node\n");
        return NULL;
    }
    
    printf("[FS_TOUCH] Created node with type=%d\n", new_file->type);
    new_file->data = (uint8_t *)kmalloc(1024); // Allocate 1KiB buffer
    new_file->size = 0;
    
    printf("[FS_TOUCH] Before fs_add_child: node type=%d\n", new_file->type);
    fs_add_child(parent, new_file);
    printf("[FS_TOUCH] After fs_add_child: node type=%d\n", new_file->type);
    
    return new_file;
}

// Free a node and all its resources
void fs_free_node(FSNode* node) {
    if (!node) return;
    
    // Free file data if it exists
    if (node->data) {
        kfree(node->data);
        node->data = NULL;
    }
    
    // Free children array if it exists (for directories)
    if (node->children) {
        kfree(node->children);
        node->children = NULL;
    }
    
    // Free the node itself
    kfree(node);
}

// Remove a file
int fs_remove(const char* path) {
    if (!path) {
        printf("[FS_REMOVE] Invalid path\n");
        return -1;
    }
    
    printf("[FS_REMOVE] Removing file: %s\n", path);
    
    // Find the file
    FSNode* node = fs_find_by_path(path);
    if (!node) {
        printf("[FS_REMOVE] File not found: %s\n", path);
        return -1;
    }
    
    // Check if it's a file
    if (node->type != FS_FILE) {
        printf("[FS_REMOVE] Path is not a file: %s (type=%d)\n", path, node->type);
        return -1;
    }
    
    // Check if file is currently open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].used && fd_table[i].node == node) {
            printf("[FS_REMOVE] Cannot remove file: %s (file is open)\n", path);
            return -1;
        }
    }
    
    // Remove from parent directory
    if (node->parent) {
        fs_remove_child(node->parent, node);
    }
    
    // Free the node
    fs_free_node(node);
    
    printf("[FS_REMOVE] File removed successfully: %s\n", path);
    return 0;
}

// Remove a directory
int fs_rmdir(const char* path) {
    if (!path) {
        printf("[FS_RMDIR] Invalid path\n");
        return -1;
    }
    
    printf("[FS_RMDIR] Removing directory: %s\n", path);
    
    // Cannot remove root directory
    if (strcmp(path, "/") == 0) {
        printf("[FS_RMDIR] Cannot remove root directory\n");
        return -1;
    }
    
    // Find the directory
    FSNode* node = fs_find_by_path(path);
    if (!node) {
        printf("[FS_RMDIR] Directory not found: %s\n", path);
        return -1;
    }
    
    // Check if it's a directory
    if (node->type != FS_DIRECTORY) {
        printf("[FS_RMDIR] Path is not a directory: %s (type=%d)\n", path, node->type);
        return -1;
    }
    
    // Check if directory is empty
    if (node->child_count > 0) {
        printf("[FS_RMDIR] Directory not empty: %s (%zu children)\n", path, node->child_count);
        return -1;
    }
    
    // Check if any files in this directory are open
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (fd_table[i].used && fd_table[i].node) {
            FSNode* open_node = fd_table[i].node;
            // Check if the open file is in this directory or subdirectory
            FSNode* current = open_node->parent;
            while (current) {
                if (current == node) {
                    printf("[FS_RMDIR] Cannot remove directory: %s (contains open files)\n", path);
                    return -1;
                }
                current = current->parent;
            }
        }
    }
    
    // Remove from parent directory
    if (node->parent) {
        fs_remove_child(node->parent, node);
    }
    
    // Free the node
    fs_free_node(node);
    
    printf("[FS_RMDIR] Directory removed successfully: %s\n", path);
    return 0;
}

#include "kernel/fat32.h"
#include "kernel/blockdev.h"
#include "kernel/heap.h"
#include <stdio.h>
#include <string.h>

static fat32_fs_t fs_info;
static fat32_file_t open_files[FAT32_MAX_OPEN_FILES];
static uint8_t mounted = 0;

int fat32_init(void) {
    printf("[FAT32] Initializing FAT32 filesystem support\n");
    
    memset(&fs_info, 0, sizeof(fs_info));
    memset(open_files, 0, sizeof(open_files));
    mounted = 0;
    
    return 0;
}

static int fat32_validate_boot_sector(fat32_boot_sector_t* boot) {
    // Check signature
    if (boot->signature != FAT32_SIGNATURE) {
        printf("[FAT32] Invalid boot sector signature: 0x%x\n", boot->signature);
        return -1;
    }
    
    // Check if it's FAT32
    if (boot->fat_size_16 != 0) {
        printf("[FAT32] Not a FAT32 filesystem (fat_size_16 != 0)\n");
        return -1;
    }
    
    if (boot->fat_size_32 == 0) {
        printf("[FAT32] Invalid FAT32 (fat_size_32 == 0)\n");
        return -1;
    }
    
    // Check bytes per sector
    if (boot->bytes_per_sector != 512) {
        printf("[FAT32] Unsupported sector size: %d\n", boot->bytes_per_sector);
        return -1;
    }
    
    printf("[FAT32] Boot sector validation passed\n");
    return 0;
}

int fat32_mount(uint8_t device_id) {
    printf("[FAT32] Mounting FAT32 filesystem on device %d\n", device_id);
    
    if (mounted) {
        printf("[FAT32] Filesystem already mounted\n");
        return -1;
    }
    
    // Read boot sector
    fat32_boot_sector_t boot_sector;
    int result = blockdev_read(device_id, 0, 1, &boot_sector);
    if (result != 0) {
        printf("[FAT32] Failed to read boot sector: %d\n", result);
        return -1;
    }
    
    printf("[FAT32] Boot sector read successfully\n");
    printf("[FAT32] OEM Name: ");
    for (int i = 0; i < 8; i++) {
        printf("%c", boot_sector.oem_name[i]);
    }
    printf("\n");
    printf("[FAT32] Bytes per sector: %d\n", boot_sector.bytes_per_sector);
    printf("[FAT32] Sectors per cluster: %d\n", boot_sector.sectors_per_cluster);
    printf("[FAT32] Reserved sectors: %d\n", boot_sector.reserved_sectors);
    printf("[FAT32] Number of FATs: %d\n", boot_sector.num_fats);
    printf("[FAT32] FAT size: %u sectors\n", boot_sector.fat_size_32);
    printf("[FAT32] Root cluster: %u\n", boot_sector.root_cluster);
    
    // Validate boot sector
    if (fat32_validate_boot_sector(&boot_sector) != 0) {
        return -1;
    }
    
    // Set up filesystem info
    fs_info.device_id = device_id;
    fs_info.bytes_per_sector = boot_sector.bytes_per_sector;
    fs_info.sectors_per_cluster = boot_sector.sectors_per_cluster;
    fs_info.reserved_sectors = boot_sector.reserved_sectors;
    fs_info.num_fats = boot_sector.num_fats;
    fs_info.fat_size = boot_sector.fat_size_32;
    fs_info.root_cluster = boot_sector.root_cluster;
    
    // Calculate important offsets
    fs_info.fat_start_sector = fs_info.reserved_sectors;
    fs_info.data_start_sector = fs_info.reserved_sectors + (fs_info.num_fats * fs_info.fat_size);
    
    // Calculate total clusters
    uint32_t total_sectors = boot_sector.total_sectors_32;
    uint32_t data_sectors = total_sectors - fs_info.data_start_sector;
    fs_info.total_clusters = data_sectors / fs_info.sectors_per_cluster;
    
    printf("[FAT32] FAT starts at sector: %u\n", fs_info.fat_start_sector);
    printf("[FAT32] Data starts at sector: %u\n", fs_info.data_start_sector);
    printf("[FAT32] Total clusters: %u\n", fs_info.total_clusters);
    
    // Allocate memory for FAT table (simplified - load entire FAT)
    uint32_t fat_bytes = fs_info.fat_size * fs_info.bytes_per_sector;
    fs_info.fat_table = (uint32_t*)kmalloc(fat_bytes);
    if (!fs_info.fat_table) {
        printf("[FAT32] Failed to allocate memory for FAT table\n");
        return -1;
    }
    
    // Read FAT table
    printf("[FAT32] Reading FAT table (%u sectors)...\n", fs_info.fat_size);
    result = blockdev_read(device_id, fs_info.fat_start_sector, 
                          fs_info.fat_size, fs_info.fat_table);
    if (result != 0) {
        printf("[FAT32] Failed to read FAT table: %d\n", result);
        kfree(fs_info.fat_table);
        return -1;
    }
    
    printf("[FAT32] FAT32 filesystem mounted successfully\n");
    mounted = 1;
    return 0;
}

int fat32_unmount(void) {
    if (!mounted) {
        return -1;
    }
    
    if (fs_info.fat_table) {
        kfree(fs_info.fat_table);
        fs_info.fat_table = NULL;
    }
    
    mounted = 0;
    printf("[FAT32] Filesystem unmounted\n");
    return 0;
}

uint32_t fat32_cluster_to_sector(uint32_t cluster) {
    if (cluster < 2) {
        return 0; // Invalid cluster
    }
    return fs_info.data_start_sector + ((cluster - 2) * fs_info.sectors_per_cluster);
}

uint32_t fat32_get_next_cluster(uint32_t cluster) {
    if (!mounted || !fs_info.fat_table) {
        return FAT32_END_CLUSTER;
    }
    
    if (cluster >= fs_info.total_clusters) {
        return FAT32_END_CLUSTER;
    }
    
    uint32_t next = fs_info.fat_table[cluster] & 0x0FFFFFFF;
    return next;
}

int fat32_read_cluster(uint32_t cluster, void* buffer) {
    if (!mounted) {
        return -1;
    }
    
    uint32_t sector = fat32_cluster_to_sector(cluster);
    if (sector == 0) {
        return -1;
    }
    
    return blockdev_read(fs_info.device_id, sector, fs_info.sectors_per_cluster, buffer);
}

int fat32_parse_dir_entry(fat32_dir_entry_t* entry, fat32_file_info_t* info) {
    // Skip deleted files
    if (entry->filename[0] == 0xE5) {
        return -1;
    }
    
    // Skip end of directory
    if (entry->filename[0] == 0x00) {
        return -1;
    }
    
    // Skip long filename entries
    if (entry->attributes == FAT32_ATTR_LONG_NAME) {
        return -1;
    }
    
    // Skip volume labels
    if (entry->attributes & FAT32_ATTR_VOLUME_ID) {
        return -1;
    }
    
    // Convert filename to null-terminated string (no extension support)
    memset(info->filename, 0, sizeof(info->filename));
    int pos = 0;
    
    // Copy all 11 characters, stopping at spaces
    for (int i = 0; i < 11 && entry->filename[i] != ' '; i++) {
        info->filename[pos++] = entry->filename[i];
    }
    
    info->attributes = entry->attributes;
    info->size = entry->file_size;
    info->cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
    
    return 0;
}

int fat32_list_directory(uint32_t dir_cluster, fat32_file_info_t* files, int max_files) {
    if (!mounted) {
        return -1;
    }
    
    printf("[FAT32] Listing directory cluster %u\n", dir_cluster);
    
    // Allocate buffer for one cluster
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        printf("[FAT32] Failed to allocate cluster buffer\n");
        return -1;
    }
    
    int file_count = 0;
    uint32_t current_cluster = dir_cluster;
    
    // Read directory cluster(s)
    while (current_cluster < FAT32_END_CLUSTER && file_count < max_files) {
        // Read the cluster
        if (fat32_read_cluster(current_cluster, cluster_buffer) != 0) {
            printf("[FAT32] Failed to read cluster %u\n", current_cluster);
            break;
        }
        
        // Parse directory entries (32 bytes each)
        uint32_t entries_per_cluster = cluster_size / 32;
        for (uint32_t i = 0; i < entries_per_cluster && file_count < max_files; i++) {
            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(cluster_buffer + (i * 32));
            
            // End of directory
            if (entry->filename[0] == 0x00) {
                goto done;
            }
            
            // Parse entry
            fat32_file_info_t info;
            if (fat32_parse_dir_entry(entry, &info) == 0) {
                if (files) {
                    files[file_count] = info;
                }
                file_count++;
                
                // Print entry info
                printf("  %s", info.filename);
                if (info.attributes & FAT32_ATTR_DIRECTORY) {
                    printf("/");
                }
                printf(" (%u bytes, cluster %u)\n", info.size, info.cluster);
            }
        }
        
        // Get next cluster in chain
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
done:
    kfree(cluster_buffer);
    printf("[FAT32] Found %d entries\n", file_count);
    return file_count;
}

uint32_t fat32_find_file(uint32_t dir_cluster, const char* filename) {
    if (!mounted) {
        return 0;
    }
    
    fat32_file_info_t files[32];
    int count = fat32_list_directory(dir_cluster, files, 32);
    
    for (int i = 0; i < count; i++) {
        if (strcmp(files[i].filename, filename) == 0) {
            return files[i].cluster;
        }
    }
    
    return 0; // Not found
}

int fat32_open(const char* path) {
    if (!mounted) {
        printf("[FAT32] No filesystem mounted\n");
        return -1;
    }
    
    // Use new path resolution to support subdirectories
    uint32_t file_cluster = fat32_find_file_by_path(path);
    if (file_cluster == 0) {
        printf("[FAT32] File not found: %s\n", path);
        return -1;
    }
    
    // Find a free file descriptor
    for (int fd = 0; fd < FAT32_MAX_OPEN_FILES; fd++) {
        if (!open_files[fd].in_use) {
            open_files[fd].start_cluster = file_cluster;
            open_files[fd].current_cluster = file_cluster;
            open_files[fd].position = 0;
            open_files[fd].cluster_position = 0;
            open_files[fd].in_use = 1;
            
            // Get file size from directory entry
            fat32_file_info_t files[32];
            int count = fat32_list_directory(fs_info.root_cluster, files, 32);
            for (int i = 0; i < count; i++) {
                if (files[i].cluster == file_cluster) {
                    open_files[fd].file_size = files[i].size;
                    break;
                }
            }
            
            printf("[FAT32] Opened file %s (fd=%d, cluster=%u, size=%u)\n", 
                   path, fd, file_cluster, open_files[fd].file_size);
            return fd;
        }
    }
    
    printf("[FAT32] No free file descriptors\n");
    return -1;
}

int fat32_read(int fd, void* buffer, size_t size) {
    if (fd < 0 || fd >= FAT32_MAX_OPEN_FILES || !open_files[fd].in_use) {
        return -1;
    }
    
    fat32_file_t* file = &open_files[fd];
    uint8_t* dest = (uint8_t*)buffer;
    size_t bytes_read = 0;
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    
    printf("[FAT32] Reading %u bytes from fd %d (pos=%u, size=%u)\n", 
           size, fd, file->position, file->file_size);
    
    // Don't read past end of file
    if (file->position >= file->file_size) {
        return 0; 
    }
    if (file->position + size > file->file_size) {
        size = file->file_size - file->position;
    }
    
    // Allocate cluster buffer
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        printf("[FAT32] Failed to allocate cluster buffer\n");
        return -1;
    }
    
    while (bytes_read < size && file->current_cluster < FAT32_END_CLUSTER) {
        // Read current cluster
        if (fat32_read_cluster(file->current_cluster, cluster_buffer) != 0) {
            printf("[FAT32] Failed to read cluster %u\n", file->current_cluster);
            kfree(cluster_buffer);
            return -1;
        }
        
        // Calculate how much to read from this cluster
        uint32_t offset_in_cluster = file->cluster_position;
        uint32_t bytes_in_cluster = cluster_size - offset_in_cluster;
        uint32_t bytes_to_read = (size - bytes_read < bytes_in_cluster) ? 
                                 (size - bytes_read) : bytes_in_cluster;
        
        // Copy data
        memcpy(dest + bytes_read, cluster_buffer + offset_in_cluster, bytes_to_read);
        bytes_read += bytes_to_read;
        file->position += bytes_to_read;
        file->cluster_position += bytes_to_read;
        
        // Move to next cluster if current one is exhausted
        if (file->cluster_position >= cluster_size) {
            file->current_cluster = fat32_get_next_cluster(file->current_cluster);
            file->cluster_position = 0;
        }
    }
    
    kfree(cluster_buffer);
    printf("[FAT32] Read %u bytes\n", bytes_read);
    return bytes_read;
}

int fat32_seek(int fd, uint32_t position) {
    if (fd < 0 || fd >= FAT32_MAX_OPEN_FILES || !open_files[fd].in_use) {
        return -1;
    }
    
    fat32_file_t* file = &open_files[fd];
    
    if (position > file->file_size) {
        position = file->file_size;
    }
    
    // Reset to beginning and seek forward
    file->current_cluster = file->start_cluster;
    file->position = 0;
    file->cluster_position = 0;
    
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    
    // Skip clusters
    while (position >= cluster_size && file->current_cluster < FAT32_END_CLUSTER) {
        file->current_cluster = fat32_get_next_cluster(file->current_cluster);
        position -= cluster_size;
        file->position += cluster_size;
    }
    
    // Set position within cluster
    file->cluster_position = position;
    file->position += position;
    
    return 0;
}

void fat32_close(int fd) {
    if (fd >= 0 && fd < FAT32_MAX_OPEN_FILES && open_files[fd].in_use) {
        open_files[fd].in_use = 0;
        printf("[FAT32] Closed file descriptor %d\n", fd);
    }
}

int fat32_get_fs_info(void) {
    if (!mounted) {
        printf("[FAT32] No filesystem mounted\n");
        return -1;
    }
    
    printf("[FAT32] Filesystem Information:\n");
    printf("  Device: %d\n", fs_info.device_id);
    printf("  Bytes per sector: %u\n", fs_info.bytes_per_sector);
    printf("  Sectors per cluster: %u\n", fs_info.sectors_per_cluster);
    printf("  Reserved sectors: %u\n", fs_info.reserved_sectors);
    printf("  Number of FATs: %u\n", fs_info.num_fats);
    printf("  FAT size: %u sectors\n", fs_info.fat_size);
    printf("  Root cluster: %u\n", fs_info.root_cluster);
    printf("  Data start sector: %u\n", fs_info.data_start_sector);
    printf("  Total clusters: %u\n", fs_info.total_clusters);
    
    return 0;
}

uint32_t fat32_get_root_cluster(void) {
    if (!mounted) {
        return 0;
    }
    return fs_info.root_cluster;
}

// Resolve a path to a directory cluster and extract the final filename
uint32_t fat32_resolve_path(const char* path, char* filename) {
    if (!mounted || !path) {
        return 0;
    }
    
    printf("[FAT32] Resolving path: %s\n", path);
    
    // Start from root directory
    uint32_t current_cluster = fs_info.root_cluster;
    
    // Handle root directory case
    if (strcmp(path, "/") == 0) {
        if (filename) strcpy(filename, "");
        return current_cluster;
    }
    
    // Skip leading slash
    if (path[0] == '/') {
        path++;
    }
    
    // Parse path components
    char component[FAT32_MAX_FILENAME + 1];
    const char* start = path;
    const char* end;
    
    while (*start) {
        // Find end of current component
        end = strchr(start, '/');
        if (!end) {
            // Last component - this is the filename
            if (filename) {
                strncpy(filename, start, FAT32_MAX_FILENAME);
                filename[FAT32_MAX_FILENAME] = '\0';
            }
            printf("[FAT32] Final component: %s (directory cluster: %u)\n", start, current_cluster);
            return current_cluster;
        }
        
        // Extract directory component
        size_t len = end - start;
        if (len > FAT32_MAX_FILENAME) {
            printf("[FAT32] Directory name too long: %.*s\n", (int)len, start);
            return 0;
        }
        
        strncpy(component, start, len);
        component[len] = '\0';
        
        printf("[FAT32] Looking for directory: %s in cluster %u\n", component, current_cluster);
        
        // Find the directory in current cluster
        uint32_t dir_cluster = fat32_find_file(current_cluster, component);
        if (dir_cluster == 0) {
            printf("[FAT32] Directory not found: %s\n", component);
            return 0;
        }
        
        // Check if it's actually a directory by looking at its entry
        fat32_file_info_t files[32];
        int count = fat32_list_directory(current_cluster, files, 32);
        int is_directory = 0;
        
        for (int i = 0; i < count; i++) {
            if (strcmp(files[i].filename, component) == 0) {
                if (files[i].attributes & FAT32_ATTR_DIRECTORY) {
                    is_directory = 1;
                }
                break;
            }
        }
        
        if (!is_directory) {
            printf("[FAT32] Path component is not a directory: %s\n", component);
            return 0;
        }
        
        current_cluster = dir_cluster;
        start = end + 1;
    }
    
    // If we get here, path ended with '/' - no filename
    if (filename) strcpy(filename, "");
    return current_cluster;
}

// Find a file by full path (supporting subdirectories)
uint32_t fat32_find_file_by_path(const char* path) {
    if (!mounted || !path) {
        return 0;
    }
    
    char filename[FAT32_MAX_FILENAME + 1];
    uint32_t dir_cluster = fat32_resolve_path(path, filename);
    
    if (dir_cluster == 0 || strlen(filename) == 0) {
        return 0;
    }
    
    printf("[FAT32] Looking for file '%s' in directory cluster %u\n", filename, dir_cluster);
    return fat32_find_file(dir_cluster, filename);
}
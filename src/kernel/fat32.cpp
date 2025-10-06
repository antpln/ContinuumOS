#include "kernel/fat32.h"
#include "kernel/blockdev.h"
#include "kernel/heap.h"
#include "kernel/debug.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static fat32_fs_t fs_info;
static fat32_file_t open_files[FAT32_MAX_OPEN_FILES];
static uint8_t mounted = 0;

static char fat32_to_upper(char ch) {
    if (ch >= 'a' && ch <= 'z') {
        return (char)(ch - ('a' - 'A'));
    }
    return ch;
}

int fat32_init(void) {
    debug("[FAT32] Initializing FAT32 filesystem support");
    
    memset(&fs_info, 0, sizeof(fs_info));
    memset(open_files, 0, sizeof(open_files));
    mounted = 0;
    
    return 0;
}

static int fat32_validate_boot_sector(fat32_boot_sector_t* boot) {
    // Check signature
    if (boot->signature != FAT32_SIGNATURE) {
        error("[FAT32] Invalid boot sector signature: 0x%x", boot->signature);
        return -1;
    }
    
    // Check if it's FAT32
    if (boot->fat_size_16 != 0) {
        error("[FAT32] Not a FAT32 filesystem (fat_size_16 != 0)");
        return -1;
    }
    
    if (boot->fat_size_32 == 0) {
        error("[FAT32] Invalid FAT32 (fat_size_32 == 0)");
        return -1;
    }
    
    // Check bytes per sector
    if (boot->bytes_per_sector != 512) {
        error("[FAT32] Unsupported sector size: %d", boot->bytes_per_sector);
        return -1;
    }
    
    success("[FAT32] Boot sector validation passed");
    return 0;
}

int fat32_mount(uint8_t device_id) {
    debug("[FAT32] Mounting FAT32 filesystem on device %d", device_id);
    
    if (mounted) {
        error("[FAT32] Filesystem already mounted");
        return -1;
    }
    
    // Read boot sector
    fat32_boot_sector_t boot_sector;
    int result = blockdev_read(device_id, 0, 1, &boot_sector);
    if (result != 0) {
        error("[FAT32] Failed to read boot sector: %d", result);
        return -1;
    }
    
    success("[FAT32] Boot sector read successfully");
    debug("[FAT32] OEM Name: %.8s", boot_sector.oem_name);
    debug("[FAT32] Bytes per sector: %d", boot_sector.bytes_per_sector);
    debug("[FAT32] Sectors per cluster: %d", boot_sector.sectors_per_cluster);
    debug("[FAT32] Reserved sectors: %d", boot_sector.reserved_sectors);
    debug("[FAT32] Number of FATs: %d", boot_sector.num_fats);
    debug("[FAT32] FAT size: %u sectors", boot_sector.fat_size_32);
    debug("[FAT32] Root cluster: %u", boot_sector.root_cluster);
    
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
    
    debug("[FAT32] FAT starts at sector: %u", fs_info.fat_start_sector);
    debug("[FAT32] Data starts at sector: %u", fs_info.data_start_sector);
    debug("[FAT32] Total clusters: %u", fs_info.total_clusters);
    
    // Allocate memory for FAT table (simplified - load entire FAT)
    uint32_t fat_bytes = fs_info.fat_size * fs_info.bytes_per_sector;
    fs_info.fat_table = (uint32_t*)kmalloc(fat_bytes);
    if (!fs_info.fat_table) {
        error("[FAT32] Failed to allocate memory for FAT table");
        return -1;
    }
    
    // Read FAT table
    debug("[FAT32] Reading FAT table (%u sectors)...", fs_info.fat_size);
    result = blockdev_read(device_id, fs_info.fat_start_sector, 
                          fs_info.fat_size, fs_info.fat_table);
    if (result != 0) {
        error("[FAT32] Failed to read FAT table: %d", result);
        kfree(fs_info.fat_table);
        return -1;
    }
    
    success("[FAT32] FAT32 filesystem mounted successfully");
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
    debug("[FAT32] Filesystem unmounted");
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

static int fat32_write_cluster(uint32_t cluster, const void* buffer) {
    if (!mounted) {
        return -1;
    }

    uint32_t sector = fat32_cluster_to_sector(cluster);
    if (sector == 0) {
        return -1;
    }

    return blockdev_write(fs_info.device_id, sector, fs_info.sectors_per_cluster, buffer);
}

static int fat32_flush_fat(void) {
    if (!mounted || !fs_info.fat_table) {
        return -1;
    }

    uint8_t* fat_bytes = (uint8_t*)fs_info.fat_table;
    uint32_t sectors = fs_info.fat_size;

    for (uint32_t copy = 0; copy < fs_info.num_fats; copy++) {
        uint32_t fat_sector = fs_info.fat_start_sector + (copy * fs_info.fat_size);
        if (blockdev_write(fs_info.device_id, fat_sector, sectors, fat_bytes) != BLOCKDEV_SUCCESS) {
            error("[FAT32] Failed to flush FAT copy %u", copy);
            return -1;
        }
    }
    return 0;
}

static uint32_t fat32_get_last_cluster(uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= FAT32_END_CLUSTER) {
        return start_cluster;
    }

    uint32_t cluster = start_cluster;
    while (1) {
        uint32_t next = fs_info.fat_table[cluster] & 0x0FFFFFFF;
        if (next < 2 || next >= FAT32_END_CLUSTER) {
            break;
        }
        cluster = next;
    }
    return cluster;
}

static uint32_t fat32_allocate_cluster(uint32_t previous_cluster) {
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* zero_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!zero_buffer) {
        error("[FAT32] Failed to allocate zero buffer for new cluster");
        return 0;
    }
    memset(zero_buffer, 0, cluster_size);

    for (uint32_t cluster = 2; cluster < fs_info.total_clusters; cluster++) {
        if ((fs_info.fat_table[cluster] & 0x0FFFFFFF) == FAT32_FREE_CLUSTER) {
            fs_info.fat_table[cluster] = FAT32_END_CLUSTER;

            if (previous_cluster >= 2 && previous_cluster < FAT32_END_CLUSTER) {
                fs_info.fat_table[previous_cluster] = (fs_info.fat_table[previous_cluster] & 0xF0000000) | cluster;
            }

            if (fat32_write_cluster(cluster, zero_buffer) != BLOCKDEV_SUCCESS) {
                error("[FAT32] Failed to clear new cluster %u", cluster);
                fs_info.fat_table[cluster] = FAT32_FREE_CLUSTER;
                kfree(zero_buffer);
                return 0;
            }

            kfree(zero_buffer);

            if (fat32_flush_fat() != 0) {
                error("[FAT32] Failed to flush FAT after allocating cluster");
                return 0;
            }

            return cluster;
        }
    }

    kfree(zero_buffer);
    error("[FAT32] No free clusters available");
    return 0;
}

static void fat32_free_cluster_chain(uint32_t start_cluster) {
    if (start_cluster < 2 || start_cluster >= FAT32_END_CLUSTER) {
        return;
    }

    uint32_t cluster = start_cluster;
    while (cluster >= 2 && cluster < FAT32_END_CLUSTER) {
        uint32_t next = fs_info.fat_table[cluster] & 0x0FFFFFFF;
        fs_info.fat_table[cluster] = FAT32_FREE_CLUSTER;
        cluster = next;
    }

    fat32_flush_fat();
}

static int fat32_directory_find_free_entry(uint32_t dir_cluster, uint32_t* out_cluster, uint32_t* out_index) {
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        return -1;
    }

    uint32_t current_cluster = dir_cluster;
    while (current_cluster >= 2 && current_cluster < FAT32_END_CLUSTER) {
        if (fat32_read_cluster(current_cluster, buffer) != 0) {
            kfree(buffer);
            return -1;
        }

        uint32_t entries_per_cluster = cluster_size / sizeof(fat32_dir_entry_t);
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;

        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            if (entries[i].filename[0] == 0x00 || entries[i].filename[0] == 0xE5) {
                if (out_cluster) {
                    *out_cluster = current_cluster;
                }
                if (out_index) {
                    *out_index = i;
                }
                kfree(buffer);
                return 0;
            }
        }

        uint32_t next = fs_info.fat_table[current_cluster] & 0x0FFFFFFF;
        if (next < 2 || next >= FAT32_END_CLUSTER) {
            // Need to extend directory
            uint32_t new_cluster = fat32_allocate_cluster(current_cluster);
            if (new_cluster == 0) {
                kfree(buffer);
                return -1;
            }
            if (out_cluster) {
                *out_cluster = new_cluster;
            }
            if (out_index) {
                *out_index = 0;
            }
            kfree(buffer);
            return 0;
        }
        current_cluster = next;
    }

    kfree(buffer);
    return -1;
}

static int fat32_build_short_name(const char* name, uint8_t out[11]) {
    memset(out, ' ', 11);

    const char* dot = strchr(name, '.');
    size_t base_len = dot ? (size_t)(dot - name) : strlen(name);
    size_t ext_len = dot ? strlen(dot + 1) : 0;

    if (base_len == 0 || base_len > 8 || ext_len > 3) {
        return -1;
    }

    for (size_t i = 0; i < base_len; i++) {
        char ch = fat32_to_upper(name[i]);
        if (ch == ' ' || ch == '.' || ch == '/' || ch == '\\') {
            return -1;
        }
        out[i] = (uint8_t)ch;
    }

    if (dot) {
        const char* ext = dot + 1;
        for (size_t i = 0; i < ext_len; i++) {
            char ch = fat32_to_upper(ext[i]);
            if (ch == ' ' || ch == '.' || ch == '/' || ch == '\\') {
                return -1;
            }
            out[8 + i] = (uint8_t)ch;
        }
    }

    return 0;
}

static int fat32_write_directory_entry(uint32_t dir_cluster, uint32_t entry_index, const fat32_dir_entry_t* entry) {
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        return -1;
    }

    uint32_t cluster = dir_cluster;
    uint32_t index = entry_index;

    while (cluster >= 2 && cluster < FAT32_END_CLUSTER) {
        if (fat32_read_cluster(cluster, buffer) != 0) {
            kfree(buffer);
            return -1;
        }

        uint32_t entries_per_cluster = cluster_size / sizeof(fat32_dir_entry_t);
        if (index < entries_per_cluster) {
            fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
            entries[index] = *entry;
            int res = fat32_write_cluster(cluster, buffer);
            kfree(buffer);
            return (res == BLOCKDEV_SUCCESS) ? 0 : -1;
        }

        index -= entries_per_cluster;
        uint32_t next = fs_info.fat_table[cluster] & 0x0FFFFFFF;
        if (next < 2 || next >= FAT32_END_CLUSTER) {
            break;
        }
        cluster = next;
    }

    kfree(buffer);
    return -1;
}

static int fat32_update_file_entry(fat32_file_t* file) {
    if (!file || file->dir_entry_cluster < 2) {
        return -1;
    }

    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        return -1;
    }

    if (fat32_read_cluster(file->dir_entry_cluster, buffer) != 0) {
        kfree(buffer);
        return -1;
    }

    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
    fat32_dir_entry_t* entry = &entries[file->dir_entry_index];

    entry->first_cluster_low = (uint16_t)(file->start_cluster & 0xFFFF);
    entry->first_cluster_high = (uint16_t)((file->start_cluster >> 16) & 0xFFFF);
    entry->file_size = file->file_size;

    int res = fat32_write_cluster(file->dir_entry_cluster, buffer);
    kfree(buffer);
    return (res == BLOCKDEV_SUCCESS) ? 0 : -1;
}

static int fat32_mark_entry_deleted(uint32_t entry_cluster, uint32_t entry_index) {
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        return -1;
    }

    if (fat32_read_cluster(entry_cluster, buffer) != 0) {
        kfree(buffer);
        return -1;
    }

    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;
    entries[entry_index].filename[0] = 0xE5;

    int res = fat32_write_cluster(entry_cluster, buffer);
    kfree(buffer);
    return (res == BLOCKDEV_SUCCESS) ? 0 : -1;
}

static int fat32_split_path(const char* path, char* parent_out, char* name_out) {
    if (!path || !parent_out || !name_out) {
        return -1;
    }

    char temp[FAT32_MAX_PATH];
    strncpy(temp, path, FAT32_MAX_PATH - 1);
    temp[FAT32_MAX_PATH - 1] = '\0';

    size_t len = strlen(temp);
    while (len > 1 && temp[len - 1] == '/') {
        temp[--len] = '\0';
    }

    char* last_slash = strrchr(temp, '/');
    if (!last_slash) {
        strcpy(parent_out, "/");
        strncpy(name_out, temp, FAT32_MAX_FILENAME);
        name_out[FAT32_MAX_FILENAME] = '\0';
        return 0;
    }

    if (last_slash == temp) {
        // Parent is root
        if (*(last_slash + 1) == '\0') {
            return -1; // path was just "/"
        }
        strcpy(parent_out, "/");
        strncpy(name_out, last_slash + 1, FAT32_MAX_FILENAME);
        name_out[FAT32_MAX_FILENAME] = '\0';
        return 0;
    }

    *last_slash = '\0';
    strncpy(parent_out, temp, FAT32_MAX_PATH - 1);
    parent_out[FAT32_MAX_PATH - 1] = '\0';

    strncpy(name_out, last_slash + 1, FAT32_MAX_FILENAME);
    name_out[FAT32_MAX_FILENAME] = '\0';
    return 0;
}

static int fat32_init_directory(uint32_t cluster, uint32_t parent_cluster) {
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* buffer = (uint8_t*)kmalloc(cluster_size);
    if (!buffer) {
        return -1;
    }

    memset(buffer, 0, cluster_size);
    fat32_dir_entry_t* entries = (fat32_dir_entry_t*)buffer;

    // "." entry pointing to this directory
    memset(&entries[0], 0, sizeof(fat32_dir_entry_t));
    memset(entries[0].filename, ' ', 11);
    entries[0].filename[0] = '.';
    entries[0].attributes = FAT32_ATTR_DIRECTORY;
    entries[0].first_cluster_low = (uint16_t)(cluster & 0xFFFF);
    entries[0].first_cluster_high = (uint16_t)((cluster >> 16) & 0xFFFF);
    entries[0].file_size = 0;

    // ".." entry pointing to parent directory (or self for root)
    memset(&entries[1], 0, sizeof(fat32_dir_entry_t));
    memset(entries[1].filename, ' ', 11);
    entries[1].filename[0] = '.';
    entries[1].filename[1] = '.';
    entries[1].attributes = FAT32_ATTR_DIRECTORY;
    uint32_t parent = (parent_cluster >= 2) ? parent_cluster : cluster;
    entries[1].first_cluster_low = (uint16_t)(parent & 0xFFFF);
    entries[1].first_cluster_high = (uint16_t)((parent >> 16) & 0xFFFF);
    entries[1].file_size = 0;

    int res = fat32_write_cluster(cluster, buffer);
    kfree(buffer);
    return (res == BLOCKDEV_SUCCESS) ? 0 : -1;
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
    
    // Convert filename to null-terminated string (supports 8.3 format)
    memset(info->filename, 0, sizeof(info->filename));
    int pos = 0;
    int base_len = 0;

    // Copy base name (first 8 characters)
    for (int i = 0; i < 8 && entry->filename[i] != ' '; i++) {
        if (pos < (int)(sizeof(info->filename) - 1)) {
            info->filename[pos++] = entry->filename[i];
            base_len++;
        }
    }

    // Copy extension (next 3 characters), prefixing with '.' if present
    int ext_len = 0;
    for (int i = 8; i < 11 && entry->filename[i] != ' '; i++) {
        if (ext_len == 0 && base_len > 0 && pos < (int)(sizeof(info->filename) - 1)) {
            info->filename[pos++] = '.';
        }
        if (pos < (int)(sizeof(info->filename) - 1)) {
            info->filename[pos++] = entry->filename[i];
        }
        ext_len++;
    }

    info->attributes = entry->attributes;
    info->size = entry->file_size;
    info->cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;

    return 0;
}

static void fat32_normalize_component(const char* input, char* output) {
    size_t out = 0;
    while (input[out] != '\0' && out < FAT32_MAX_FILENAME) {
        char ch = input[out];
        if (ch == '.') {
            output[out] = '.';
        } else {
            output[out] = fat32_to_upper(ch);
        }
        out++;
    }
    output[out] = '\0';
}

static int fat32_find_entry_in_directory(uint32_t dir_cluster, const char* name,
                                         fat32_file_info_t* info,
                                         uint32_t* entry_cluster,
                                         uint32_t* entry_index) {
    if (!mounted || !name || name[0] == '\0') {
        return -1;
    }

    char search[FAT32_MAX_FILENAME + 1];
    fat32_normalize_component(name, search);

    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        error("[FAT32] Failed to allocate cluster buffer for lookup");
        return -1;
    }

    uint32_t current_cluster = dir_cluster;

    while (current_cluster < FAT32_END_CLUSTER) {
        if (fat32_read_cluster(current_cluster, cluster_buffer) != 0) {
            error("[FAT32] Failed to read cluster %u during lookup", current_cluster);
            kfree(cluster_buffer);
            return -1;
        }

        uint32_t entries_per_cluster = cluster_size / sizeof(fat32_dir_entry_t);
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry_t* entry = (fat32_dir_entry_t*)(cluster_buffer + (i * sizeof(fat32_dir_entry_t)));

            if (entry->filename[0] == 0x00) {
                kfree(cluster_buffer);
                return -1; // End of directory
            }

            fat32_file_info_t entry_info;
            if (fat32_parse_dir_entry(entry, &entry_info) == 0) {
                char normalized[FAT32_MAX_FILENAME + 1];
                fat32_normalize_component(entry_info.filename, normalized);
                if (strcmp(normalized, search) == 0) {
                    if (info) {
                        *info = entry_info;
                    }
                    if (entry_cluster) {
                        *entry_cluster = current_cluster;
                    }
                    if (entry_index) {
                        *entry_index = i;
                    }
                    kfree(cluster_buffer);
                    return 0;
                }
            }
        }

        current_cluster = fat32_get_next_cluster(current_cluster);
    }

    kfree(cluster_buffer);
    return -1;
}

int fat32_lookup_path(const char* path, fat32_file_info_t* info,
                      uint32_t* parent_dir_cluster,
                      uint32_t* entry_cluster,
                      uint32_t* entry_index) {
    if (!mounted || !path) {
        return -1;
    }

    if (info) {
        memset(info, 0, sizeof(*info));
    }
    if (parent_dir_cluster) {
        *parent_dir_cluster = fs_info.root_cluster;
    }
    if (entry_cluster) {
        *entry_cluster = fs_info.root_cluster;
    }
    if (entry_index) {
        *entry_index = 0;
    }

    if (path[0] == '\0' || strcmp(path, "/") == 0) {
        if (info) {
            strcpy(info->filename, "/");
            info->attributes = FAT32_ATTR_DIRECTORY;
            info->size = 0;
            info->cluster = fs_info.root_cluster;
        }
        return 0;
    }

    char working_path[FAT32_MAX_PATH];
    strncpy(working_path, path, FAT32_MAX_PATH - 1);
    working_path[FAT32_MAX_PATH - 1] = '\0';

    char* cursor = working_path;
    if (*cursor == '/') {
        cursor++;
    }

    uint32_t current_dir = fs_info.root_cluster;
    uint32_t parent_dir = fs_info.root_cluster;
    fat32_file_info_t last_entry;
    uint32_t last_entry_cluster = fs_info.root_cluster;
    uint32_t last_entry_index = 0;
    int found_any = 0;

    while (*cursor != '\0') {
        while (*cursor == '/') {
            cursor++;
        }
        if (*cursor == '\0') {
            break;
        }

        char* slash = strchr(cursor, '/');
        size_t len = slash ? (size_t)(slash - cursor) : strlen(cursor);
        if (len == 0) {
            cursor = slash ? slash + 1 : cursor + len;
            continue;
        }

        if (len > FAT32_MAX_FILENAME) {
            error("[FAT32] Path component too long: %.*s", (int)len, cursor);
            return -1;
        }

        char component[FAT32_MAX_FILENAME + 1];
        strncpy(component, cursor, len);
        component[len] = '\0';

        if (strcmp(component, ".") == 0) {
            cursor = slash ? slash + 1 : cursor + len;
            continue;
        }

        if (strcmp(component, "..") == 0) {
            return -1; // Parent traversal handled by VFS layer
        }

        fat32_file_info_t entry_info;
        uint32_t found_cluster = 0;
        uint32_t found_index = 0;
        if (fat32_find_entry_in_directory(current_dir, component, &entry_info, &found_cluster, &found_index) != 0) {
            return -1;
        }

        found_any = 1;
        parent_dir = current_dir;
        last_entry = entry_info;
        last_entry_cluster = found_cluster;
        last_entry_index = found_index;

        if (slash) {
            if (!(entry_info.attributes & FAT32_ATTR_DIRECTORY)) {
                return -1;
            }
            current_dir = entry_info.cluster;
            cursor = slash + 1;
            continue;
        } else {
            cursor += len;
            break;
        }
    }

    if (!found_any) {
        return -1;
    }

    if (info) {
        *info = last_entry;
    }
    if (parent_dir_cluster) {
        *parent_dir_cluster = parent_dir;
    }
    if (entry_cluster) {
        *entry_cluster = last_entry_cluster;
    }
    if (entry_index) {
        *entry_index = last_entry_index;
    }

    return 0;
}

int fat32_list_directory(uint32_t dir_cluster, fat32_file_info_t* files, int max_files) {
    if (!mounted) {
        return -1;
    }
    
    debug("[FAT32] Listing directory cluster %u", dir_cluster);
    
    // Allocate buffer for one cluster
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;
    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        error("[FAT32] Failed to allocate cluster buffer");
        return -1;
    }
    
    int file_count = 0;
    uint32_t current_cluster = dir_cluster;
    
    // Read directory cluster(s)
    while (current_cluster < FAT32_END_CLUSTER && file_count < max_files) {
        // Read the cluster
        if (fat32_read_cluster(current_cluster, cluster_buffer) != 0) {
            error("[FAT32] Failed to read cluster %u", current_cluster);
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
                if (strcmp(info.filename, ".") == 0 || strcmp(info.filename, "..") == 0) {
                    continue;
                }
                if (files) {
                    files[file_count] = info;
                }
                file_count++;
                
                // Print entry info
                debug("  %s%s (%u bytes, cluster %u)", 
                      info.filename,
                      (info.attributes & FAT32_ATTR_DIRECTORY) ? "/" : "",
                      info.size, info.cluster);
            }
        }
        
        // Get next cluster in chain
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
done:
    kfree(cluster_buffer);
    debug("[FAT32] Found %d entries", file_count);
    return file_count;
}

uint32_t fat32_find_file(uint32_t dir_cluster, const char* filename) {
    if (!mounted) {
        return 0;
    }
    fat32_file_info_t entry;
    if (fat32_find_entry_in_directory(dir_cluster, filename, &entry, NULL, NULL) == 0) {
        return entry.cluster;
    }
    return 0;
}

int fat32_open(const char* path) {
    if (!mounted) {
        error("[FAT32] No filesystem mounted");
        return -1;
    }
    
    fat32_file_info_t info;
    uint32_t parent_dir = 0;
    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;

    if (fat32_lookup_path(path, &info, &parent_dir, &entry_cluster, &entry_index) != 0) {
        error("[FAT32] File not found: %s", path);
        return -1;
    }

    if (info.attributes & FAT32_ATTR_DIRECTORY) {
        error("[FAT32] Cannot open directory as file: %s", path);
        return -1;
    }

    uint32_t file_cluster = info.cluster;
    
    // Find a free file descriptor
    for (int fd = 0; fd < FAT32_MAX_OPEN_FILES; fd++) {
        if (!open_files[fd].in_use) {
            open_files[fd].start_cluster = file_cluster;
            open_files[fd].current_cluster = file_cluster;
            open_files[fd].last_cluster = (file_cluster >= 2 && file_cluster < FAT32_END_CLUSTER)
                                          ? fat32_get_last_cluster(file_cluster)
                                          : file_cluster;
            open_files[fd].position = 0;
            open_files[fd].cluster_position = 0;
            open_files[fd].file_size = info.size;
            open_files[fd].dir_cluster = parent_dir;
            open_files[fd].dir_entry_cluster = entry_cluster;
            open_files[fd].dir_entry_index = entry_index;
            open_files[fd].in_use = 1;
            
            success("[FAT32] Opened file %s (fd=%d, cluster=%u, size=%u)", 
                    path, fd, file_cluster, open_files[fd].file_size);
            return fd;
        }
    }
    
    error("[FAT32] No free file descriptors");
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
    
    debug("[FAT32] Reading %u bytes from fd %d (pos=%u, size=%u)", 
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
        error("[FAT32] Failed to allocate cluster buffer");
        return -1;
    }
    
    while (bytes_read < size && file->current_cluster < FAT32_END_CLUSTER) {
        // Read current cluster
        if (fat32_read_cluster(file->current_cluster, cluster_buffer) != 0) {
            error("[FAT32] Failed to read cluster %u", file->current_cluster);
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
    debug("[FAT32] Read %u bytes", bytes_read);
    return bytes_read;
}

int fat32_write(int fd, const void* buffer, size_t size) {
    if (fd < 0 || fd >= FAT32_MAX_OPEN_FILES || !open_files[fd].in_use) {
        return -1;
    }

    if (size == 0) {
        return 0;
    }

    fat32_file_t* file = &open_files[fd];
    const uint8_t* src = (const uint8_t*)buffer;
    uint32_t cluster_size = fs_info.sectors_per_cluster * fs_info.bytes_per_sector;

    uint8_t* cluster_buffer = (uint8_t*)kmalloc(cluster_size);
    if (!cluster_buffer) {
        error("[FAT32] Failed to allocate cluster buffer for write");
        return -1;
    }

    size_t bytes_written = 0;
    while (bytes_written < size) {
        if (file->start_cluster < 2) {
            uint32_t allocated = fat32_allocate_cluster(file->start_cluster);
            if (allocated == 0) {
                kfree(cluster_buffer);
                return bytes_written > 0 ? (int)bytes_written : -1;
            }
            file->start_cluster = allocated;
            file->current_cluster = allocated;
            file->last_cluster = allocated;
            file->cluster_position = 0;
        }

        if (file->current_cluster < 2 || file->current_cluster >= FAT32_END_CLUSTER) {
            uint32_t new_cluster = fat32_allocate_cluster(file->last_cluster >= 2 ? file->last_cluster : file->start_cluster);
            if (new_cluster == 0) {
                kfree(cluster_buffer);
                return bytes_written > 0 ? (int)bytes_written : -1;
            }
            if (file->last_cluster >= 2 && file->last_cluster < FAT32_END_CLUSTER) {
                file->current_cluster = new_cluster;
            } else {
                file->current_cluster = file->start_cluster;
            }
            file->last_cluster = new_cluster;
            file->cluster_position = 0;
        }

        if (file->cluster_position >= cluster_size) {
            uint32_t next = fs_info.fat_table[file->current_cluster] & 0x0FFFFFFF;
            if (next < 2 || next >= FAT32_END_CLUSTER) {
                uint32_t new_cluster = fat32_allocate_cluster(file->current_cluster);
                if (new_cluster == 0) {
                    kfree(cluster_buffer);
                    return bytes_written > 0 ? (int)bytes_written : -1;
                }
                file->last_cluster = new_cluster;
                next = new_cluster;
            }
            file->current_cluster = next;
            file->cluster_position = 0;
        }

        size_t offset = file->cluster_position;
        size_t available = cluster_size - offset;
        size_t to_write = size - bytes_written;
        if (to_write > available) {
            to_write = available;
        }

        bool need_read = (offset != 0 || to_write < cluster_size);

        if (need_read) {
            if (fat32_read_cluster(file->current_cluster, cluster_buffer) != 0) {
                kfree(cluster_buffer);
                return bytes_written > 0 ? (int)bytes_written : -1;
            }
        } else {
            memset(cluster_buffer, 0, cluster_size);
        }

        memcpy(cluster_buffer + offset, src + bytes_written, to_write);

        if (fat32_write_cluster(file->current_cluster, cluster_buffer) != BLOCKDEV_SUCCESS) {
            kfree(cluster_buffer);
            return bytes_written > 0 ? (int)bytes_written : -1;
        }

        file->cluster_position += to_write;
        file->position += to_write;
        bytes_written += to_write;

        if (file->position > file->file_size) {
            file->file_size = file->position;
        }
    }

    kfree(cluster_buffer);

    if (fat32_update_file_entry(file) != 0) {
        error("[FAT32] Failed to update directory entry after write");
        return -1;
    }

    return (int)bytes_written;
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
        open_files[fd].start_cluster = 0;
        open_files[fd].current_cluster = 0;
        open_files[fd].last_cluster = 0;
        open_files[fd].file_size = 0;
        open_files[fd].position = 0;
        open_files[fd].cluster_position = 0;
        open_files[fd].dir_cluster = 0;
        open_files[fd].dir_entry_cluster = 0;
        open_files[fd].dir_entry_index = 0;
        debug("[FAT32] Closed file descriptor %d", fd);
    }
}

int fat32_get_fs_info(void) {
    if (!mounted) {
        error("[FAT32] No filesystem mounted");
        return -1;
    }
    
    debug("[FAT32] Filesystem Information:");
    debug("  Device: %d", fs_info.device_id);
    debug("  Bytes per sector: %u", fs_info.bytes_per_sector);
    debug("  Sectors per cluster: %u", fs_info.sectors_per_cluster);
    debug("  Reserved sectors: %u", fs_info.reserved_sectors);
    debug("  Number of FATs: %u", fs_info.num_fats);
    debug("  FAT size: %u sectors", fs_info.fat_size);
    debug("  Root cluster: %u", fs_info.root_cluster);
    debug("  Data start sector: %u", fs_info.data_start_sector);
    debug("  Total clusters: %u", fs_info.total_clusters);
    
    return 0;
}

uint32_t fat32_get_root_cluster(void) {
    if (!mounted) {
        return 0;
    }
    return fs_info.root_cluster;
}

// Find a file by full path (supporting subdirectories)
uint32_t fat32_find_file_by_path(const char* path) {
    if (!mounted || !path) {
        return 0;
    }

    fat32_file_info_t info;
    if (fat32_lookup_path(path, &info, NULL, NULL, NULL) != 0) {
        return 0;
    }

    return info.cluster;
}

int fat32_create(const char* path) {
    if (!mounted || !path || path[0] == '\0') {
        return -1;
    }

    if (path[0] != '/') {
        error("[FAT32] Only absolute paths are supported for create");
        return -1;
    }

    fat32_file_info_t existing;
    if (fat32_lookup_path(path, &existing, NULL, NULL, NULL) == 0) {
        error("[FAT32] File already exists: %s", path);
        return -1;
    }

    char parent[FAT32_MAX_PATH];
    char name[FAT32_MAX_FILENAME + 1];
    if (fat32_split_path(path, parent, name) != 0) {
        error("[FAT32] Invalid path: %s", path);
        return -1;
    }

    if (strlen(name) == 0) {
        return -1;
    }

    if (strcmp(parent, "") == 0) {
        strcpy(parent, "/");
    }

    fat32_file_info_t dir_info;
    if (fat32_lookup_path(parent, &dir_info, NULL, NULL, NULL) != 0) {
        error("[FAT32] Parent directory not found: %s", parent);
        return -1;
    }

    if (!(dir_info.attributes & FAT32_ATTR_DIRECTORY)) {
        error("[FAT32] Parent is not a directory: %s", parent);
        return -1;
    }

    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;
    if (fat32_directory_find_free_entry(dir_info.cluster, &entry_cluster, &entry_index) != 0) {
        error("[FAT32] No space in directory: %s", parent);
        return -1;
    }

    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    if (fat32_build_short_name(name, entry.filename) != 0) {
        error("[FAT32] Unsupported filename: %s", name);
        return -1;
    }

    entry.attributes = FAT32_ATTR_ARCHIVE;
    entry.first_cluster_low = 0;
    entry.first_cluster_high = 0;
    entry.file_size = 0;

    if (fat32_write_directory_entry(entry_cluster, entry_index, &entry) != 0) {
        error("[FAT32] Failed to write directory entry for %s", name);
        return -1;
    }

    return 0;
}

int fat32_remove(const char* path) {
    if (!mounted || !path || path[0] == '\0') {
        return -1;
    }

    fat32_file_info_t info;
    uint32_t parent_dir = 0;
    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;

    if (fat32_lookup_path(path, &info, &parent_dir, &entry_cluster, &entry_index) != 0) {
        error("[FAT32] File not found: %s", path);
        return -1;
    }

    if (info.attributes & FAT32_ATTR_DIRECTORY) {
        error("[FAT32] Path is a directory (use rmdir): %s", path);
        return -1;
    }

    if (info.cluster >= 2) {
        fat32_free_cluster_chain(info.cluster);
    }

    if (fat32_mark_entry_deleted(entry_cluster, entry_index) != 0) {
        error("[FAT32] Failed to remove directory entry: %s", path);
        return -1;
    }

    return 0;
}

int fat32_mkdir_path(const char* path) {
    if (!mounted || !path || path[0] == '\0') {
        return -1;
    }

    if (path[0] != '/') {
        error("[FAT32] Only absolute paths are supported for mkdir");
        return -1;
    }

    if (strcmp(path, "/") == 0) {
        return -1; // root already exists
    }

    fat32_file_info_t existing;
    if (fat32_lookup_path(path, &existing, NULL, NULL, NULL) == 0) {
        error("[FAT32] Directory already exists: %s", path);
        return -1;
    }

    char parent[FAT32_MAX_PATH];
    char name[FAT32_MAX_FILENAME + 1];
    if (fat32_split_path(path, parent, name) != 0) {
        return -1;
    }

    if (strlen(name) == 0) {
        return -1;
    }

    if (strcmp(parent, "") == 0) {
        strcpy(parent, "/");
    }

    fat32_file_info_t dir_info;
    if (fat32_lookup_path(parent, &dir_info, NULL, NULL, NULL) != 0) {
        error("[FAT32] Parent directory not found: %s", parent);
        return -1;
    }

    if (!(dir_info.attributes & FAT32_ATTR_DIRECTORY)) {
        error("[FAT32] Parent is not a directory: %s", parent);
        return -1;
    }

    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;
    if (fat32_directory_find_free_entry(dir_info.cluster, &entry_cluster, &entry_index) != 0) {
        error("[FAT32] No space in directory: %s", parent);
        return -1;
    }

    uint32_t new_cluster = fat32_allocate_cluster(0);
    if (new_cluster == 0) {
        return -1;
    }

    if (fat32_init_directory(new_cluster, dir_info.cluster) != 0) {
        fat32_free_cluster_chain(new_cluster);
        return -1;
    }

    fat32_dir_entry_t entry;
    memset(&entry, 0, sizeof(entry));
    if (fat32_build_short_name(name, entry.filename) != 0) {
        error("[FAT32] Unsupported directory name: %s", name);
        return -1;
    }

    entry.attributes = FAT32_ATTR_DIRECTORY;
    entry.first_cluster_low = (uint16_t)(new_cluster & 0xFFFF);
    entry.first_cluster_high = (uint16_t)((new_cluster >> 16) & 0xFFFF);
    entry.file_size = 0;

    if (fat32_write_directory_entry(entry_cluster, entry_index, &entry) != 0) {
        error("[FAT32] Failed to write directory entry for mkdir: %s", name);
        return -1;
    }

    return 0;
}

int fat32_rmdir_path(const char* path) {
    if (!mounted || !path || path[0] == '\0') {
        return -1;
    }

    if (strcmp(path, "/") == 0) {
        return -1; // cannot remove root
    }

    fat32_file_info_t info;
    uint32_t parent_dir = 0;
    uint32_t entry_cluster = 0;
    uint32_t entry_index = 0;

    if (fat32_lookup_path(path, &info, &parent_dir, &entry_cluster, &entry_index) != 0) {
        error("[FAT32] Directory not found: %s", path);
        return -1;
    }

    if (!(info.attributes & FAT32_ATTR_DIRECTORY)) {
        error("[FAT32] Path is not a directory: %s", path);
        return -1;
    }

    fat32_file_info_t temp[1];
    int entry_count = fat32_list_directory(info.cluster, temp, 1);
    if (entry_count < 0) {
        return -1;
    }
    if (entry_count > 0) {
        error("[FAT32] Directory not empty: %s", path);
        return -1;
    }

    if (info.cluster >= 2) {
        fat32_free_cluster_chain(info.cluster);
    }

    if (fat32_mark_entry_deleted(entry_cluster, entry_index) != 0) {
        error("[FAT32] Failed to remove directory entry: %s", path);
        return -1;
    }

    return 0;
}

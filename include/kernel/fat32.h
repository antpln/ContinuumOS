#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>

// FAT32 constants
#define FAT32_SIGNATURE     0xAA55
#define FAT32_BOOT_SIG      0x29
#define FAT32_END_CLUSTER   0x0FFFFFFF
#define FAT32_BAD_CLUSTER   0x0FFFFFF7
#define FAT32_FREE_CLUSTER  0x00000000

// Directory entry attributes
#define FAT32_ATTR_READ_ONLY 0x01
#define FAT32_ATTR_HIDDEN    0x02
#define FAT32_ATTR_SYSTEM    0x04
#define FAT32_ATTR_VOLUME_ID 0x08
#define FAT32_ATTR_DIRECTORY 0x10
#define FAT32_ATTR_ARCHIVE   0x20
#define FAT32_ATTR_LONG_NAME 0x0F

// Maximum values
#define FAT32_MAX_FILENAME   255
#define FAT32_MAX_PATH       256
#define FAT32_MAX_OPEN_FILES 16

// Packed structures for on-disk format
typedef struct __attribute__((packed)) {
    uint8_t  jump_boot[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t total_sectors_16;
    uint8_t  media_type;
    uint16_t fat_size_16;
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    uint32_t fat_size_32;
    uint16_t ext_flags;
    uint16_t fs_version;
    uint32_t root_cluster;
    uint16_t fs_info;
    uint16_t backup_boot_sector;
    uint8_t  reserved[12];
    uint8_t  drive_number;
    uint8_t  reserved1;
    uint8_t  boot_signature;
    uint32_t volume_id;
    uint8_t  volume_label[11];
    uint8_t  file_system_type[8];
    uint8_t  boot_code[420];
    uint16_t signature;
} fat32_boot_sector_t;

typedef struct __attribute__((packed)) {
    uint8_t  filename[11];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_tenths;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t first_cluster_high;
    uint16_t write_time;
    uint16_t write_date;
    uint16_t first_cluster_low;
    uint32_t file_size;
} fat32_dir_entry_t;

typedef struct __attribute__((packed)) {
    uint8_t  order;
    uint16_t name1[5];
    uint8_t  attributes;
    uint8_t  type;
    uint8_t  checksum;
    uint16_t name2[6];
    uint16_t first_cluster_low;
    uint16_t name3[2];
} fat32_lfn_entry_t;

// Runtime structures
typedef struct {
    uint8_t device_id;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t reserved_sectors;
    uint32_t num_fats;
    uint32_t fat_size;
    uint32_t root_cluster;
    uint32_t data_start_sector;
    uint32_t fat_start_sector;
    uint32_t total_clusters;
    uint32_t* fat_table;
} fat32_fs_t;

typedef struct {
    uint32_t start_cluster;
    uint32_t current_cluster;
    uint32_t file_size;
    uint32_t position;
    uint32_t cluster_position;
    uint32_t last_cluster;       // Last cluster in the chain (for appends)
    uint32_t dir_cluster;        // Directory containing this file's entry
    uint32_t dir_entry_cluster;  // Cluster where the directory entry resides
    uint32_t dir_entry_index;    // Index within the directory cluster
    uint8_t in_use;
} fat32_file_t;

typedef struct {
    char filename[FAT32_MAX_FILENAME + 1];
    char short_name[13];
    uint8_t has_long_name;
    uint8_t attributes;
    uint32_t size;
    uint32_t cluster;
} fat32_file_info_t;

// Function declarations
int fat32_init(void);
int fat32_mount(uint8_t device_id);
int fat32_unmount(void);
int fat32_open(const char* path);
int fat32_read(int fd, void* buffer, size_t size);
int fat32_write(int fd, const void* buffer, size_t size);
int fat32_seek(int fd, uint32_t position);
void fat32_close(int fd);
int fat32_list_directory(uint32_t dir_cluster, fat32_file_info_t* files, int max_files);
uint32_t fat32_find_file(uint32_t dir_cluster, const char* filename);
uint32_t fat32_find_file_by_path(const char* path);
int fat32_lookup_path(const char* path, fat32_file_info_t* info,
                      uint32_t* parent_dir_cluster,
                      uint32_t* entry_cluster,
                      uint32_t* entry_index);
int fat32_create(const char* path);
int fat32_remove(const char* path);
int fat32_mkdir_path(const char* path);
int fat32_rmdir_path(const char* path);
int fat32_get_fs_info(void);
uint32_t fat32_get_root_cluster(void);

// Internal functions
uint32_t fat32_cluster_to_sector(uint32_t cluster);
uint32_t fat32_get_next_cluster(uint32_t cluster);
int fat32_read_cluster(uint32_t cluster, void* buffer);

#endif // FAT32_H

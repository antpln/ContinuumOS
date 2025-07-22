#ifndef BLOCKDEV_H
#define BLOCKDEV_H

#include <stdint.h>
#include <stddef.h>

// Block device types
#define BLOCKDEV_TYPE_IDE    1
#define BLOCKDEV_TYPE_FLOPPY 2
#define BLOCKDEV_TYPE_USB    3

// Error codes
#define BLOCKDEV_SUCCESS     0
#define BLOCKDEV_ERROR      -1
#define BLOCKDEV_NOT_FOUND  -2
#define BLOCKDEV_NO_MEDIA   -3

// Maximum block devices
#define MAX_BLOCK_DEVICES    8

typedef struct {
    uint8_t type;
    uint8_t device_id;
    uint32_t sector_count;
    uint16_t sector_size;
    uint8_t present;
    char name[16];
} blockdev_info_t;

// Function declarations
int blockdev_init(void);
int blockdev_register(uint8_t type, uint8_t device_id, blockdev_info_t* info);
int blockdev_read(uint8_t device, uint32_t sector, uint8_t count, void* buffer);
int blockdev_write(uint8_t device, uint32_t sector, uint8_t count, const void* buffer);
blockdev_info_t* blockdev_get_info(uint8_t device);
int blockdev_list_devices(void);

#endif // BLOCKDEV_H
#include "kernel/blockdev.h"
#include "kernel/ide.h"
#include "kernel/debug.h"
#include <stdio.h>
#include <string.h>

static blockdev_info_t devices[MAX_BLOCK_DEVICES];
static uint8_t device_count = 0;

int blockdev_init(void) {
    debug("[BLOCKDEV] Initializing block device subsystem");
    
    device_count = 0;
    memset(devices, 0, sizeof(devices));
    
    // Initialize IDE subsystem
    int ide_drives = ide_init();
    
    // Register IDE drives as block devices
    for (int i = 0; i < ide_drives && device_count < MAX_BLOCK_DEVICES; i++) {
        ide_drive_t* drive = ide_get_drive(i);
        if (drive && drive->exists) {
            blockdev_info_t info;
            info.type = BLOCKDEV_TYPE_IDE;
            info.device_id = i;
            info.sector_count = drive->sectors;
            info.sector_size = 512;
            info.present = 1;
            strcpy(info.name, "hd");
            info.name[2] = '0' + i;
            info.name[3] = '\0';
            
            blockdev_register(BLOCKDEV_TYPE_IDE, i, &info);
        }
    }
    
    success("[BLOCKDEV] Registered %d block devices", device_count);
    return device_count;
}

int blockdev_register(uint8_t type, uint8_t device_id, blockdev_info_t* info) {
    if (device_count >= MAX_BLOCK_DEVICES) {
        error("[BLOCKDEV] Maximum devices reached");
        return BLOCKDEV_ERROR;
    }
    
    devices[device_count] = *info;
    debug("[BLOCKDEV] Registered device %d: %s (%u sectors, %u bytes/sector)",
           device_count, info->name, info->sector_count, info->sector_size);
    
    device_count++;
    return BLOCKDEV_SUCCESS;
}

int blockdev_read(uint8_t device, uint32_t sector, uint8_t count, void* buffer) {
    if (device >= device_count || !devices[device].present) {
        error("[BLOCKDEV] Invalid device: %d", device);
        return BLOCKDEV_NOT_FOUND;
    }
    
    blockdev_info_t* dev = &devices[device];
    
    if (sector >= dev->sector_count) {
        error("[BLOCKDEV] Sector %u out of range (max: %u)", sector, dev->sector_count - 1);
        return BLOCKDEV_ERROR;
    }
    
    debug("[BLOCKDEV] Reading %d sectors from sector %u on device %d (%s)",
           count, sector, device, dev->name);
    
    switch (dev->type) {
        case BLOCKDEV_TYPE_IDE:
            return ide_read_sectors(dev->device_id, sector, count, (uint16_t*)buffer);
        
        default:
            error("[BLOCKDEV] Unsupported device type: %d", dev->type);
            return BLOCKDEV_ERROR;
    }
}

int blockdev_write(uint8_t device, uint32_t sector, uint8_t count, const void* buffer) {
    if (device >= device_count || !devices[device].present) {
        return BLOCKDEV_NOT_FOUND;
    }
    
    blockdev_info_t* dev = &devices[device];
    
    switch (dev->type) {
        case BLOCKDEV_TYPE_IDE:
            return ide_write_sectors(dev->device_id, sector, count, (uint16_t*)buffer);
        
        default:
            return BLOCKDEV_ERROR;
    }
}

blockdev_info_t* blockdev_get_info(uint8_t device) {
    if (device >= device_count || !devices[device].present) {
        return NULL;
    }
    return &devices[device];
}

int blockdev_list_devices(void) {
    debug("[BLOCKDEV] Available block devices:");
    for (int i = 0; i < device_count; i++) {
        if (devices[i].present) {
            debug("  %d: %s - %u sectors (%u MB)",
                   i, devices[i].name, devices[i].sector_count,
                   (devices[i].sector_count * devices[i].sector_size) / (1024 * 1024));
        }
    }
    return device_count;
}
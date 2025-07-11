#include "kernel/ide.h"
#include "kernel/port_io.h"
#include <stdio.h>
#include <string.h>

static ide_drive_t drives[IDE_MAX_DRIVES];
static uint8_t drive_count = 0;

static void ide_delay(uint16_t base_port) {
    // 400ns delay by reading status register 4 times
    for (int i = 0; i < 4; i++) {
        inb(base_port + IDE_REG_STATUS);
    }
}

static int ide_wait_ready(uint16_t base_port) {
    uint8_t status;
    int timeout = 1000000;
    
    while (timeout--) {
        status = inb(base_port + IDE_REG_STATUS);
        if (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_RDY)) {
            return 0;
        }
        if (status & IDE_STATUS_ERR) {
            printf("[IDE] Error status: 0x%x\n", status);
            return -1;
        }
    }
    printf("[IDE] Timeout waiting for ready\n");
    return -1;
}

static int ide_wait_drq(uint16_t base_port) {
    uint8_t status;
    int timeout = 1000000;
    
    while (timeout--) {
        status = inb(base_port + IDE_REG_STATUS);
        if (!(status & IDE_STATUS_BSY) && (status & IDE_STATUS_DRQ)) {
            return 0;
        }
        if (status & IDE_STATUS_ERR) {
            printf("[IDE] Error status during DRQ wait: 0x%x\n", status);
            return -1;
        }
    }
    printf("[IDE] Timeout waiting for DRQ\n");
    return -1;
}

static void ide_select_drive(uint16_t base_port, uint8_t drive) {
    uint8_t drive_select = 0xE0 | ((drive & 1) << 4);
    outb(base_port + IDE_REG_DRIVE, drive_select);
    ide_delay(base_port);
}

int ide_identify(uint8_t drive_id, uint16_t* buffer) {
    if (drive_id >= IDE_MAX_DRIVES) {
        return -1;
    }
    
    ide_drive_t* drive = &drives[drive_id];
    uint16_t base_port = drive->base_port;
    uint8_t drive_num = drive->drive_num;
    
    ide_select_drive(base_port, drive_num);
    
    if (ide_wait_ready(base_port) != 0) {
        return -1;
    }
    
    // Send IDENTIFY command
    outb(base_port + IDE_REG_COMMAND, IDE_CMD_IDENTIFY);
    ide_delay(base_port);
    
    // Check if drive exists
    uint8_t status = inb(base_port + IDE_REG_STATUS);
    if (status == 0) {
        return -1; // No drive
    }
    
    if (ide_wait_drq(base_port) != 0) {
        return -1;
    }
    
    // Read 256 words (512 bytes) of identification data
    for (int i = 0; i < 256; i++) {
        buffer[i] = inw(base_port + IDE_REG_DATA);
    }
    
    return 0;
}

int ide_read_sectors(uint8_t drive_id, uint32_t lba, uint8_t count, uint16_t* buffer) {
    if (drive_id >= IDE_MAX_DRIVES || !drives[drive_id].exists) {
        printf("[IDE] Invalid drive: %d\n", drive_id);
        return -1;
    }
    
    if (count == 0 || count > 256) {
        printf("[IDE] Invalid sector count: %d\n", count);
        return -1;
    }
    
    ide_drive_t* drive = &drives[drive_id];
    uint16_t base_port = drive->base_port;
    uint8_t drive_num = drive->drive_num;
    
    printf("[IDE] Reading %d sectors from LBA %u on drive %d\n", count, lba, drive_id);
    
    ide_select_drive(base_port, drive_num);
    
    if (ide_wait_ready(base_port) != 0) {
        return -1;
    }
    
    // Set up LBA addressing
    outb(base_port + IDE_REG_FEATURES, 0x00);
    outb(base_port + IDE_REG_SECTOR_COUNT, count);
    outb(base_port + IDE_REG_LBA_LOW, lba & 0xFF);
    outb(base_port + IDE_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(base_port + IDE_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    
    // Set drive and upper LBA bits (LBA mode)
    uint8_t drive_select = 0xE0 | ((drive_num & 1) << 4) | ((lba >> 24) & 0x0F);
    outb(base_port + IDE_REG_DRIVE, drive_select);
    
    // Send READ SECTORS command
    outb(base_port + IDE_REG_COMMAND, IDE_CMD_READ_SECTORS);
    
    // Read each sector
    for (int sector = 0; sector < count; sector++) {
        if (ide_wait_drq(base_port) != 0) {
            printf("[IDE] Failed to read sector %d\n", sector);
            return -1;
        }
        
        // Read 256 words (512 bytes) per sector
        for (int i = 0; i < 256; i++) {
            buffer[sector * 256 + i] = inw(base_port + IDE_REG_DATA);
        }
    }
    
    printf("[IDE] Successfully read %d sectors\n", count);
    return 0;
}

int ide_write_sectors(uint8_t drive_id, uint32_t lba, uint8_t count, uint16_t* buffer) {
    // Write functionality - to be implemented later
    printf("[IDE] Write functionality not yet implemented\n");
    return -1;
}

static int ide_detect_drive(uint16_t base_port, uint8_t drive_num) {
    uint16_t identify_buffer[256];
    
    ide_drive_t temp_drive;
    temp_drive.base_port = base_port;
    temp_drive.drive_num = drive_num;
    
    // Temporarily set up drive info for identify
    drives[drive_count] = temp_drive;
    
    if (ide_identify(drive_count, identify_buffer) == 0) {
        // Drive exists, parse identify data
        drives[drive_count].exists = 1;
        drives[drive_count].cylinders = identify_buffer[1];
        drives[drive_count].heads = identify_buffer[3];
        drives[drive_count].sectors_per_track = identify_buffer[6];
        
        // Calculate total sectors (LBA mode)
        drives[drive_count].sectors = *((uint32_t*)&identify_buffer[60]);
        
        printf("[IDE] Drive %d detected: %u sectors (%u MB)\n", 
               drive_count, drives[drive_count].sectors,
               (drives[drive_count].sectors * 512) / (1024 * 1024));
        
        drive_count++;
        return 1;
    } else {
        drives[drive_count].exists = 0;
        return 0;
    }
}

int ide_init(void) {
    printf("[IDE] Initializing IDE controller\n");
    
    drive_count = 0;
    memset(drives, 0, sizeof(drives));
    
    // Detect drives on primary IDE controller
    printf("[IDE] Scanning primary IDE controller (0x1F0)\n");
    ide_detect_drive(IDE_PRIMARY_BASE, IDE_DRIVE_MASTER);
    ide_detect_drive(IDE_PRIMARY_BASE, IDE_DRIVE_SLAVE);
    
    // Detect drives on secondary IDE controller
    printf("[IDE] Scanning secondary IDE controller (0x170)\n");
    ide_detect_drive(IDE_SECONDARY_BASE, IDE_DRIVE_MASTER);
    ide_detect_drive(IDE_SECONDARY_BASE, IDE_DRIVE_SLAVE);
    
    printf("[IDE] Found %d drives\n", drive_count);
    return drive_count;
}

ide_drive_t* ide_get_drive(uint8_t drive_id) {
    if (drive_id >= IDE_MAX_DRIVES || !drives[drive_id].exists) {
        return NULL;
    }
    return &drives[drive_id];
}
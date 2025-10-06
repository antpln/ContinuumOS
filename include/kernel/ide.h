#ifndef IDE_H
#define IDE_H

#include <stdint.h>
#include <stddef.h>

// IDE I/O ports
#define IDE_PRIMARY_BASE    0x1F0
#define IDE_SECONDARY_BASE  0x170

// IDE register offsets
#define IDE_REG_DATA        0x00
#define IDE_REG_FEATURES    0x01
#define IDE_REG_SECTOR_COUNT 0x02
#define IDE_REG_LBA_LOW     0x03
#define IDE_REG_LBA_MID     0x04
#define IDE_REG_LBA_HIGH    0x05
#define IDE_REG_DRIVE       0x06
#define IDE_REG_COMMAND     0x07
#define IDE_REG_STATUS      0x07

// IDE commands
#define IDE_CMD_READ_SECTORS 0x20
#define IDE_CMD_WRITE_SECTORS 0x30
#define IDE_CMD_IDENTIFY     0xEC

// IDE status bits
#define IDE_STATUS_ERR       0x01
#define IDE_STATUS_DRQ       0x08
#define IDE_STATUS_SRV       0x10
#define IDE_STATUS_DF        0x20
#define IDE_STATUS_RDY       0x40
#define IDE_STATUS_BSY       0x80

// Drive selection
#define IDE_DRIVE_MASTER     0x00
#define IDE_DRIVE_SLAVE      0x01

// Maximum drives
#define IDE_MAX_DRIVES       4

typedef struct {
    uint16_t base_port;
    uint8_t drive_num;
    uint8_t exists;
    uint32_t sectors;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;
} ide_drive_t;

// Function declarations
int ide_init(void);
int ide_read_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t* buffer);
int ide_write_sectors(uint8_t drive, uint32_t lba, uint8_t count, uint16_t* buffer);
int ide_identify(uint8_t drive, uint16_t* buffer);
ide_drive_t* ide_get_drive(uint8_t drive);

#endif // IDE_H
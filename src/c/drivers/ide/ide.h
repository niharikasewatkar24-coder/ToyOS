#ifndef IDE_H
#define IDE_H

#include "../../kernel/kernel.h"

#define IDE_DATA       0x1F0
#define IDE_ERROR      0x1F1
#define IDE_SECCOUNT   0x1F2
#define IDE_LBA_LOW    0x1F3
#define IDE_LBA_MID    0x1F4
#define IDE_LBA_HIGH   0x1F5
#define IDE_DRIVE      0x1F6
#define IDE_STATUS     0x1F7
#define IDE_ALT_STATUS 0x3F6
#define IDE_COMMAND    0x1F7

#define IDE_READ       0x20
#define IDE_WRITE      0x30
#define IDE_FLUSH      0xE7

#define IDE_STATUS_BSY 0x80
#define IDE_STATUS_DRQ 0x08
#define IDE_STATUS_ERR 0x01

int ide_read_sector(u32 lba, u16 *buffer);
int ide_write_sector(u32 lba, const u16 *buffer);

#endif
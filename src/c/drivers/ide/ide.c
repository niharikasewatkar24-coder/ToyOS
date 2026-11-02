
#include "ide.h"

#define IDE_TIMEOUT 1000000

static int ide_wait_ready(void)
{
    u32 timeout = IDE_TIMEOUT;
    u8 status;

    while (timeout--)
    {
        status = in(IDE_STATUS);

        if (status == 0)
            return -1;

        if (!(status & IDE_STATUS_BSY))
        {
            if (status & IDE_STATUS_ERR)
                return -1;

            return 0;
        }
    }

    return -1;
}

static int ide_wait_drq(void)
{
    u32 timeout = IDE_TIMEOUT;
    u8 status;

    while (timeout--)
    {
        status = in(IDE_STATUS);

        if (status == 0 || (status & IDE_STATUS_ERR))
            return -1;

        if (!(status & IDE_STATUS_BSY) &&
            (status & IDE_STATUS_DRQ))
            return 0;
    }

    return -1;
}

static void ide_select_drive(u32 lba)
{
    out(IDE_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
}

int ide_read_sector(u32 lba, u16 *buffer)
{
    int i;

    if (buffer == 0 || lba >= 2048)
        return -1;

    if (ide_wait_ready() != 0)
        return -1;

    ide_select_drive(lba);

    out(IDE_SECCOUNT, 1);
    out(IDE_LBA_LOW, (u8)lba);
    out(IDE_LBA_MID, (u8)(lba >> 8));
    out(IDE_LBA_HIGH, (u8)(lba >> 16));
    out(IDE_COMMAND, IDE_READ);

    if (ide_wait_drq() != 0)
        return -1;

    for (i = 0; i < 256; i++)
        buffer[i] = inw(IDE_DATA);

    return 0;
}

int ide_write_sector(u32 lba, const u16 *buffer)
{
    int i;

    if (buffer == 0 || lba >= 2048)
        return -1;

    if (ide_wait_ready() != 0)
        return -1;

    ide_select_drive(lba);

    out(IDE_SECCOUNT, 1);
    out(IDE_LBA_LOW, (u8)lba);
    out(IDE_LBA_MID, (u8)(lba >> 8));
    out(IDE_LBA_HIGH, (u8)(lba >> 16));
    out(IDE_COMMAND, IDE_WRITE);

    if (ide_wait_drq() != 0)
        return -1;

    for (i = 0; i < 256; i++)
        outw(IDE_DATA, buffer[i]);

    /* Wait until the sector write completes */
    if (ide_wait_ready() != 0)
        return -1;

    /* Flush disk write cache */
    out(IDE_COMMAND, IDE_FLUSH);

    return ide_wait_ready();
}

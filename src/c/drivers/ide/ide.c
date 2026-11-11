
#include "ide.h"
#include "../serial_port/serial_port.h"

#define IDE_TIMEOUT 1000000

static int ide_wait_ready(void)
{
    u32 timeout = IDE_TIMEOUT;
    u8 status;

    while (timeout--)
    {
        status = in(IDE_STATUS);

        if (status == 0)
        {
            serial_log(LOG_ERROR, "IDE READY FAILED: STATUS ZERO");
            return -1;
        }

        if (!(status & IDE_STATUS_BSY))
        {
            if (status & IDE_STATUS_ERR)
            {
                (void)in(IDE_ERROR);
                serial_log(LOG_ERROR, "IDE READY FAILED: ATA ERROR");
                return -1;
            }

            return 0;
        }
    }

    serial_log(LOG_ERROR, "IDE READY FAILED: BUSY TIMEOUT");
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

static void ide_400ns_delay(void)
{
    (void)in(IDE_ALT_STATUS);
    (void)in(IDE_ALT_STATUS);
    (void)in(IDE_ALT_STATUS);
    (void)in(IDE_ALT_STATUS);
}

static void ide_select_drive(u32 lba)
{
    out(IDE_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    ide_400ns_delay();
}

int ide_read_sector(u32 lba, u16 *buffer)
{
    int i;

    if (buffer == 0 || lba >= 2048)
        return -1;

    ide_select_drive(lba);

    if (ide_wait_ready() != 0)
        return -1;

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
    {
        serial_log(LOG_ERROR, "IDE WRITE FAILED: INVALID ARGUMENT");
        return -1;
    }

    ide_select_drive(lba);

    if (ide_wait_ready() != 0)
    {
        serial_log(LOG_ERROR, "IDE WRITE FAILED: INITIAL DRIVE READY");
        return -1;
    }

    out(IDE_SECCOUNT, 1);
    out(IDE_LBA_LOW, (u8)lba);
    out(IDE_LBA_MID, (u8)(lba >> 8));
    out(IDE_LBA_HIGH, (u8)(lba >> 16));
    out(IDE_COMMAND, IDE_WRITE);

    if (ide_wait_drq() != 0)
    {
        serial_log(LOG_ERROR, "IDE WRITE FAILED: WAITING FOR DRQ");
        return -1;
    }

    for (i = 0; i < 256; i++)
        outw(IDE_DATA, buffer[i]);

    if (ide_wait_ready() != 0)
    {
        serial_log(LOG_ERROR, "IDE WRITE FAILED: WRITE COMPLETION");
        return -1;
    }

    out(IDE_COMMAND, IDE_FLUSH);

    if (ide_wait_ready() != 0)
    {
        serial_log(LOG_ERROR, "IDE WRITE FAILED: CACHE FLUSH");
        return -1;
    }

    return 0;
}

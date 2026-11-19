/**
 * @file ata.c
 * @author Joe Bayer (joexbayer)
 * @brief ATA driver for non volatile storage. Using PIO for simplicity.
 * @see https://wiki.osdev.org/PCI_IDE_Controller
 * @version 0.1
 * @date 2022-06-24
 * 
 * @copyright Copyright (c) 2022
 * 
 */

#include <ata.h>
#include <arch/interrupts.h>
#include <memory.h>
#include <arch/io.h>
#include <diskdev.h>
#include <libc.h>

#include <kutils.h>
#include <sync.h>

#include <serial.h>

static uint8_t* ata_driver_data;
struct ide_device ata_ide_device;

/* The channels, by ATA_PRIMARY and ATA_SECONDARY: their command blocks, at the legacy ports */
static const struct {
	uint16_t io;
	const char* name;
} ata_channels[] = {
	[ATA_PRIMARY] = { ATA_PRIMARY_IO, "primary" },
	[ATA_SECONDARY] = { ATA_SECONDARY_IO, "secondary" },
};

/* The command block of the channel the disk is on: its master. Set when ata_ide_init finds it. */
static uint16_t ata_io = ATA_PRIMARY_IO;

/* One transfer at a time. A lock, not interrupts off, so the rest of the system keeps running during one (#196).
 * acquire can block, so the driver can't be called with interrupts off or from an interrupt handler. */
static mutex_t ata_lock;

void __int_handler ata_primary()
{
}

void __int_handler ata_secondary()
{
}

/**
 * How many times a wait reads the drive's status before giving up. Drivers attach in init_pci, before the timer starts
 * and with interrupts off, so waits count reads instead of time: a read of an I/O port takes about a microsecond on
 * real hardware, so this is a few seconds. The BIOS has already spun the drive up by the time the kernel runs.
 */
#define ATA_WAIT_POLLS 5000000

/* Waits until the drive isn't busy. Returns its status then, or -1 if it's still busy after ATA_WAIT_POLLS reads. */
static int ata_status_wait(int io_base) {
	for(int i = 0; i < ATA_WAIT_POLLS; i++){
		uint8_t status = inportb(io_base + ATA_REG_STATUS);
		if(!(status & ATA_SR_BSY)) return status;
	}
	return -1;
}

static void ata_io_wait(int io_base) {
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
	inportb(io_base + ATA_REG_ALTSTATUS);
}

/* Waits until the drive isn't busy, and with adv, checks it has data ready without an error. Returns 1 if not. */
static int ata_wait(int io, int adv)
{
    ata_io_wait(io);

    int status = ata_status_wait(io);
    if (status < 0) return 1;

    if (adv) {
        if (status & ATA_SR_ERR) return 1;
        if (status & ATA_SR_DF)  return 1;
        if (!(status & ATA_SR_DRQ)) return 1;
    }

    return 0;
}

static int __ata_read_sector(char *buf, int lba)
{
    uint16_t io = ata_io;

    uint8_t cmd = 0xE0;
    int errors = 0;

__ata_read_sector_try_again:
    outportb(io + ATA_REG_CONTROL, 0x02);

    ata_wait(io, 0);

    outportb(io + ATA_REG_HDDEVSEL, (cmd | (uint8_t)((lba >> 24 & 0x0F))));
    outportb(io + ATA_REG_FEATURES, 0x00);
    outportb(io + ATA_REG_SECCOUNT0, 1);
    outportb(io + ATA_REG_LBA0, (uint8_t)(lba));
    outportb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outportb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outportb(io + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    if (ata_wait(io, 1)) {
        errors ++;
        if (errors > 4)
            return -1;

        goto __ata_read_sector_try_again;
    }

    for (int i = 0; i < 256; i++) {
        uint16_t d = inportw(io + ATA_REG_DATA);
        *(uint16_t *)(buf + i * 2) = d;
    }

    ata_wait(io, 0);
    return 0;
}

/**
 * Waits for the drive to finish a command, after giving it 400 ns to start. Returns -1 if it reports an error or a
 * fault, or is still busy after ATA_WAIT_POLLS reads.
 */
static int ata_wait_done(int io)
{
    ata_io_wait(io);

    int status = ata_status_wait(io);
    if (status < 0 || (status & (ATA_SR_ERR | ATA_SR_DF))) return -1;
    return 0;
}

/**
 * Writes a sector. With the drive's write cache on, it's in the cache when this returns, not yet on the disk:
 * ata_flush puts it there, when FatFs syncs (#219). Each step waits for the drive: it asks for the data with DRQ,
 * then is busy while it stores it, and it ignores a command written while it's busy, as the next one would be if it
 * came right after the data. Returns 0, or -1 if the drive reports an error or a fault, or stays busy.
 */
static int __ata_write_sector(uint16_t *buf, int lba)
{
    uint16_t io = ata_io;

    outportb(io + ATA_REG_CONTROL, 0x02);
    if (ata_wait(io, 0)) return -1;

    outportb(io + ATA_REG_HDDEVSEL, 0xE0 | (uint8_t)((lba >> 24) & 0x0F));
    if (ata_wait(io, 0)) return -1;

    outportb(io + ATA_REG_FEATURES, 0x00);
    outportb(io + ATA_REG_SECCOUNT0, 1);
    outportb(io + ATA_REG_LBA0, (uint8_t)(lba));
    outportb(io + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outportb(io + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outportb(io + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    /* Ready for the data, without an error */
    if (ata_wait(io, 1)) return -1;

    for (int i = 0; i < 256; i++) {
        outportw(io + ATA_REG_DATA, buf[i]);
    }

    if (ata_wait_done(io) < 0) return -1;

    return 0;
}

/**
 * Flushes the drive's write cache, so every sector written before it is on the disk when this returns. FatFs asks
 * for it at the end of every sync, through disk_flush, so that's once per fs_write rather than once per sector, as
 * it used to be (#219). The drive is busy until the cache is written, which is all of one fs_write: a few MB at most
 * is well inside ATA_WAIT_POLLS. Returns 0, or -1 if the drive reports an error or a fault, or stays busy.
 */
int ata_flush(void)
{
    uint16_t io = ata_io;
    int ret = -1;

    acquire(&ata_lock);

    outportb(io + ATA_REG_CONTROL, 0x02);
    if (!ata_wait(io, 0)) {
        outportb(io + ATA_REG_HDDEVSEL, 0xE0);
        if (!ata_wait(io, 0)) {
            outportb(io + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
            ret = ata_wait_done(io);
        }
    }

    release(&ata_lock);

    if (ret < 0) dbgprintf("[ATA]: Flushing the drive's cache failed.\n");
    return ret;
}

/* Writes count sectors from sector from. Returns count, or -1 at the first that fails, as FatFs's disk_write expects */
int ata_write(char *buf, uint32_t from, uint32_t count)
{
    acquire(&ata_lock);

    for (uint32_t i = 0; i < count; i++)
    {
        if (__ata_write_sector((uint16_t*) buf, from + i) < 0) {
            release(&ata_lock);
            dbgprintf("[ATA]: Writing sector %d failed.\n", from + i);
            return -1;
        }
        buf += 512;
    }

    release(&ata_lock);
    return count;
}

int ata_read(char *buf, uint32_t from, uint32_t numsects)
{
	unsigned long pos = from;
    int rc = 0;

    acquire(&ata_lock);

    for (uint32_t i = 0; i < numsects; i++)
    {
        rc = __ata_read_sector((char*) buf, pos + i);
        if (rc == -1){
            release(&ata_lock);
            return -1;
        }
        buf += 512;
    }

    release(&ata_lock);
    return rc;
}

/* Why __ata_identify found no drive, by the negative number it returns */
static const char* ata_identify_errors[] = {
	[1] = "nothing answers",
	[2] = "it stays busy",
	[3] = "it isn't an ATA disk",
	[4] = "it reports an error",
};

/**
 * Looks for an ATA disk at a channel's master with IDENTIFY, and reads its 256 words of identity into ata_driver_data.
 * Returns 0 if there's one, or the negative index of ata_identify_errors: a status of 0, or 0xff from a channel with
 * nothing on it, means nothing answers; an ATAPI or SATAPI signature in LBA1 and LBA2, as an optical drive's, isn't
 * an ATA disk. Every wait has a limit, so an empty or broken channel can't hang the boot.
 */
static int __ata_identify(int io)
{
	outportb(io + ATA_REG_HDDEVSEL, 0xA0);
	ata_io_wait(io);
	outportb(io + ATA_REG_CONTROL, 0x02);	/* nIEN: the driver polls */

	if(inportb(io + ATA_REG_STATUS) == 0xFF) return -1;

	outportb(io + ATA_REG_SECCOUNT0, 0);
	outportb(io + ATA_REG_LBA0, 0);
	outportb(io + ATA_REG_LBA1, 0);
	outportb(io + ATA_REG_LBA2, 0);
	outportb(io + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
	ata_io_wait(io);

	uint8_t status = inportb(io + ATA_REG_STATUS);
	if(status == 0 || status == 0xFF) return -1;

	if(ata_status_wait(io) < 0) return -2;

	if(inportb(io + ATA_REG_LBA1) != 0 || inportb(io + ATA_REG_LBA2) != 0) return -3;

	for(int i = 0; ; i++){
		if(i == ATA_WAIT_POLLS) return -2;
		status = inportb(io + ATA_REG_STATUS);
		if(status & ATA_SR_ERR) return -4;
		if(status & ATA_SR_DRQ) break;
	}

	for(int i = 0; i < 256; i++){
		*(uint16_t *)(ata_driver_data + i*2) = inportw(io + ATA_REG_DATA);
	}
	return 0;
}

void ata_ide_init(struct pci_device* dev)
{
	interrupt_install_handler(ATA_PRIMARY_IRQ, &ata_primary);
	interrupt_install_handler(ATA_SECONDARY_IRQ, &ata_secondary);

	ata_driver_data = kalloc(512);
	if(ata_driver_data == NULL){
		dbgprintf("[ATA]: Failed to allocate memory for driver data.\n");
		return;
	}

	/**
	 * The disk is the first ATA disk found, on the primary channel's master, then the secondary's: a SATA controller
	 * in IDE mode puts a drive on each channel's master, and RetrOS has one disk device. Slaves aren't looked for.
	 */
	/**
	 * The programming interface says which channels are in native mode, at ports the BIOS assigned (bits 0 and 2, for
	 * the primary and secondary) rather than the legacy ones the driver uses. Such a channel is passed over.
	 */
	uint8_t progif = (pci_read_word(dev->bus, dev->slot, dev->function, 0x08) >> 8) & 0xFF;

	int channel = -1;
	for(int c = ATA_PRIMARY; c <= ATA_SECONDARY && channel < 0; c++){
		if(progif & (1 << (c*2))){
			dbgprintf("[ATA]: The %s channel is in native mode, which the driver doesn't support.\n", ata_channels[c].name);
			continue;
		}
		int found = __ata_identify(ata_channels[c].io);
		if(found == 0){
			channel = c;
		} else {
			dbgprintf("[ATA]: No disk on the %s channel's master: %s.\n", ata_channels[c].name, ata_identify_errors[-found]);
		}
	}
	if(channel < 0) return;

	ata_io = ata_channels[channel].io;
	dbgprintf("[ATA]: Driver ready, with the disk on the %s channel's master.\n", ata_channels[channel].name);

	for(int i = 0; i < ATA_IDE_MODEL_LENGTH-1; i += 2){
		ata_ide_device.model[i] = ata_driver_data[ATA_IDENT_MODEL + i + 1];
		ata_ide_device.model[i + 1] = ata_driver_data[ATA_IDENT_MODEL + i];
	}

	ata_ide_device.channel = channel;
	ata_ide_device.drive = ATA_MASTER;
	ata_ide_device.signature = *((unsigned short *)(ata_driver_data + ATA_IDENT_DEVICETYPE));
	ata_ide_device.commandSets = *((unsigned int *)(ata_driver_data + ATA_IDENT_COMMANDSETS));

	if (ata_ide_device.commandSets & (1 << 26))
		/* Device uses 48-Bit Addressing */
		ata_ide_device.size = *((unsigned int *)(ata_driver_data + ATA_IDENT_MAX_LBA_EXT));
	else
		/* Device uses CHS or 28-bit Addressing */
		ata_ide_device.size = *((unsigned int *)(ata_driver_data + ATA_IDENT_MAX_LBA));

	mutex_init(&ata_lock);
	attach_disk_dev(&ata_read, &ata_write, &ata_flush, &ata_ide_device);
}

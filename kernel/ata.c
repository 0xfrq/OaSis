#include "ata.h"
#include "io.h"
#include <stdint.h>

static int ata_present = 0;

/* 400ns delay */
static inline void ata_400ns_delay(void) {
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

/* Wait until BSY cleared */
static int ata_wait_not_busy(uint32_t spins) {
    while (spins--) {
        uint8_t s = inb(ATA_STATUS);
        if (s == 0xFF) return 0;        // floating bus
        if (!(s & ATA_STATUS_BSY)) return 1;
    }
    return 0;
}

/* Wait until DRQ set */
static int ata_wait_drq(uint32_t spins) {
    while (spins--) {
        uint8_t s = inb(ATA_STATUS);
        if (s == 0xFF) return 0;
        if (s & ATA_STATUS_ERR) return 0;
        if (s & ATA_STATUS_DRQ) return 1;
    }
    return 0;
}

/* -------------------------------------------------- */
/* ATA INIT — IDENTIFY BASED (CORRECT WAY)             */
/* -------------------------------------------------- */
void ata_init(void) {
    uint16_t identify[256];

    ata_present = 0;

    /* Select primary master */
    outb(ATA_DRIVE, ATA_MASTER);
    ata_400ns_delay();

    /* Zero registers per ATA spec */
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    /* Send IDENTIFY */
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    /* If status = 0x00 or 0xFF → no device */
    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF)
        return;

    if (!ata_wait_drq(2000000))
        return;

    /* Read IDENTIFY data */
    for (int i = 0; i < 256; i++)
        identify[i] = inw(ATA_DATA);

    ata_present = 1;
}

/* Single source of truth */
int ata_is_present(void) {
    return ata_present;
}

/* -------------------------------------------------- */
/* READ SECTOR                                        */
/* -------------------------------------------------- */
int ata_read_sector(uint32_t lba, uint8_t *buf) {
    if (!ata_present) return -1;
    if (!ata_wait_not_busy(1000000)) return -1;

    outb(ATA_DRIVE, ATA_MASTER | ((lba >> 24) & 0x0F));
    ata_400ns_delay();
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq(2000000)) return -1;

    for (int i = 0; i < 256; i++) {
        uint16_t w = inw(ATA_DATA);
        buf[i * 2]     = w & 0xFF;
        buf[i * 2 + 1] = w >> 8;
    }

    return 0;
}

/* -------------------------------------------------- */
/* WRITE SECTOR                                       */
/* -------------------------------------------------- */
int ata_write_sector(uint32_t lba, const uint8_t *buf) {
    if (!ata_present) return -1;
    if (!ata_wait_not_busy(1000000)) return -1;

    outb(ATA_DRIVE, ATA_MASTER | ((lba >> 24) & 0x0F));
    ata_400ns_delay();
    outb(ATA_SECTOR_COUNT, 1);
    outb(ATA_LBA_LOW, lba & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8) & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_WRITE_SECTORS);

    if (!ata_wait_drq(2000000)) return -1;

    for (int i = 0; i < 256; i++) {
        uint16_t w = buf[i * 2] | (buf[i * 2 + 1] << 8);
        outw(ATA_DATA, w);
    }

    if (!ata_wait_not_busy(2000000)) return -1;
    return 0;
}

/* -------------------------------------------------- */
/* IDENTIFY (USER CALL)                               */
/* -------------------------------------------------- */
int ata_identify(uint16_t *buf) {
    if (!ata_present) return -1;

    outb(ATA_DRIVE, ATA_MASTER);
    ata_400ns_delay();
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    if (!ata_wait_drq(2000000)) return -1;

    for (int i = 0; i < 256; i++)
        buf[i] = inw(ATA_DATA);

    return 0;
}

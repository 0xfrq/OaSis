#include "ata.h"
#include "io.h"
#include <stdint.h>

static int ata_present = 0;

/* delay 400ns */
static inline void ata_400ns_delay(void) {
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

/* tunggu sampe BSY ilang */
static int ata_wait_not_busy(uint32_t spins) {
    while (spins--) {
        uint8_t s = inb(ATA_STATUS);
        if (s == 0xFF) return 0;        // bus floating
        if (!(s & ATA_STATUS_BSY)) return 1;
    }
    return 0;
}

/* tunggu sampe DRQ nyala */
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
/* ATA INIT — pake IDENTIFY (cara yang bener)         */
/* -------------------------------------------------- */
void ata_init(void) {
    uint16_t identify[256];

    ata_present = 0;

    /* pilih primary master */
    outb(ATA_DRIVE, ATA_MASTER);
    ata_400ns_delay();

    /* nol-in register sesuai spek ATA */
    outb(ATA_SECTOR_COUNT, 0);
    outb(ATA_LBA_LOW, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HIGH, 0);

    /* kirim perintah IDENTIFY */
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    /* kalo status = 0x00 atau 0xFF → gak ada device */
    uint8_t status = inb(ATA_STATUS);
    if (status == 0x00 || status == 0xFF)
        return;

    if (!ata_wait_drq(2000000))
        return;

    /* baca data IDENTIFY */
    for (int i = 0; i < 256; i++)
        identify[i] = inw(ATA_DATA);

    ata_present = 1;
}

/* satu sumber kebenaran aja */
int ata_is_present(void) {
    return ata_present;
}

/* -------------------------------------------------- */
/* BACA SEKTOR                                        */
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
/* TULIS SEKTOR                                       */
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
/* IDENTIFY (panggilan dari user)                     */
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

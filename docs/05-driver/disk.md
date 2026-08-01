# Disk driver

this page explains how OaSis reads and writes the hard disk.

## Contents

- [overview](#overview)
- [ata/ide](#ataide)
- [port i/o](#port-io)
- [read sector](#read-sector)
- [write sector](#write-sector)
- [api reference](#api-reference)

---

## Overview

**disk driver** handles ATA/IDE hard-disk reads and writes.

### Capabilities

- read sector (512 bytes)
- write sector (512 bytes)
- detect disk presence
- PIO mode (programmed I/O)

### Limitation

- does not support yet DMA (more lambat from PIO)
- does not support yet LBA48 (max 128 GB)
- does not support yet SATA/AHCI
- does not support yet caching

## Ata/ide

**ATA (Advanced Technology Attachment)** is standard for hard disk.

### Karakteristik

- **sector size**: 512 bytes
- **addressing**: LBA (Logical block Addressing)
- **mode**: PIO (Programmed I/O) - CPU read/write directly

### Primary ata bus

OaSis use primary ATA bus:

```text
I/O ports: 0x1F0 - 0x1F7
IRQ: 14
```

## Port i/o

ATA use beberapa port I/O:

| port | read | write | deskripsi |
|------|------|-------|-----------|
| 0x1F0 | data | data | data port (16-bit) |
| 0x1F1 | error | features | error/features |
| 0x1F2 | sector count | sector count | number sector |
| 0x1F3 | lba low | lba low | LBA bits 0-7 |
| 0x1F4 | lba mid | lba mid | LBA bits 8-15 |
| 0x1F5 | lba high | lba high | LBA bits 16-23 |
| 0x1F6 | drive/head | drive/head | drive select + LBA bits 24-27 |
| 0x1F7 | status | command | status/command register |

### Status register

```text
bit 0: ERR (error occurred)
bit 1: IDX (index mark)
bit 2: CORR (corrected data)
bit 3: DRQ (data request - ready to transfer)
bit 4: SRV (service request)
bit 5: DF (drive fault)
bit 6: RDY (drive ready)
bit 7: BSY (busy)
```

### Command register

```text
0x20: read sector (PIO)
0x30: write sector (PIO)
0xEC: identify drive
```

## Read sector

### Flow

```text
set sector count
  ↓
set LBA address
  ↓
send read command (0x20)
  ↓
wait for DRQ (data request)
  ↓
read 256 words (512 bytes)
  ↓
done
```

### Implementasi

```c
int ata_read_sector(uint32_t lba, void *buffer) {
    // wait for drive ready
    if (!ata_wait_ready()) {
        return -1;
    }
    
    // set sector count
    outb(0x1F2, 1);
    
    // set LBA address
    outb(0x1F3, lba & 0xFF);           // LBA low
    outb(0x1F4, (lba >> 8) & 0xFF);    // LBA mid
    outb(0x1F5, (lba >> 16) & 0xFF);   // LBA high
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));  // drive 0 + LBA high
    
    // send read command
    outb(0x1F7, 0x20);
    
    // wait for DRQ
    if (!ata_wait_drq()) {
        return -1;
    }
    
    // read 256 words (512 bytes)
    uint16_t *buf = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        buf[i] = inw(0x1F0);
    }
    
    return 0;
}
```

## Write sector

### Flow

```text
set sector count
  ↓
set LBA address
  ↓
send write command (0x30)
  ↓
wait for DRQ (data request)
  ↓
write 256 words (512 bytes)
  ↓
flush cache
  ↓
done
```

### Implementasi

```c
int ata_write_sector(uint32_t lba, const void *buffer) {
    // wait for drive ready
    if (!ata_wait_ready()) {
        return -1;
    }
    
    // set sector count
    outb(0x1F2, 1);
    
    // set LBA address
    outb(0x1F3, lba & 0xFF);
    outb(0x1F4, (lba >> 8) & 0xFF);
    outb(0x1F5, (lba >> 16) & 0xFF);
    outb(0x1F6, 0xE0 | ((lba >> 24) & 0x0F));
    
    // send write command
    outb(0x1F7, 0x30);
    
    // wait for DRQ
    if (!ata_wait_drq()) {
        return -1;
    }
    
    // write 256 words (512 bytes)
    const uint16_t *buf = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(0x1F0, buf[i]);
    }
    
    // flush cache
    outb(0x1F7, 0xE7);
    ata_wait_ready();
    
    return 0;
}
```

## Api reference

### Initialization

```c
void ata_init(void);
```

initialization ATA driver. detect disk presence.

### Detect disk

```c
int ata_detect(void);
```

detect whether a disk is present on the primary ATA channel.

**return:**
- `0`: disk detected
- `-1`: no disk

### Read sector

```c
int ata_read_sector(uint32_t lba, void *buffer);
```

read one sector (512 bytes) from disk.

**parameter:**
- `lba`: logical block address
- `buffer`: buffer for store data (minimal 512 bytes)

**return:**
- `0`: success
- `-1`: error

**example:**
```c
uint8_t buffer[512];
if (ata_read_sector(0, buffer) == 0) {
    // process data
}
```

### Write sector

```c
int ata_write_sector(uint32_t lba, const void *buffer);
```

write one sector (512 bytes) to disk.

**parameter:**
- `lba`: logical block address
- `buffer`: data that mau ditulis (512 bytes)

**return:**
- `0`: success
- `-1`: error

**example:**
```c
uint8_t data[512];
// fill data
if (ata_write_sector(0, data) == 0) {
    // success
}
```

### Read multiple sectors

```c
int ata_read_sectors(uint32_t lba, uint32_t count, void *buffer);
```

read multiple sectors.

**parameter:**
- `lba`: start LBA
- `count`: number sector
- `buffer`: buffer (minimal count * 512 bytes)

**return:**
- `0`: success
- `-1`: error

### Write multiple sectors

```c
int ata_write_sectors(uint32_t lba, uint32_t count, const void *buffer);
```

write multiple sectors.

**parameter:**
- `lba`: start LBA
- `count`: number sector
- `buffer`: data (count * 512 bytes)

**return:**
- `0`: success
- `-1`: error

---

## Helper functions

### Wait ready

```c
int ata_wait_ready(void) {
    while (1) {
        uint8_t status = inb(0x1F7);
        if (!(status & 0x80)) {  // not busy
            if (status & 0x40) {  // ready
                return 0;
            }
            if (status & 0x01) {  // error
                return -1;
            }
        }
    }
}
```

### Wait drq

```c
int ata_wait_drq(void) {
    while (1) {
        uint8_t status = inb(0x1F7);
        if (status & 0x08) {  // DRQ
            return 0;
        }
        if (status & 0x01) {  // error
            return -1;
        }
        if (status & 0x20) {  // drive fault
            return -1;
        }
    }
}
```

---

## Troubleshooting

### Disk not detected

- check that the ATA controller is enabled in BIOS or QEMU
- cek port 0x1F7 (harus can dibaca)
- cek drive select byte (0xE0 for drive 0)

### Read/write error

- cek status register (bit 0 = error)
- cek LBA address valid
- cek buffer size (minimal 512 bytes)

### Data corrupt

- cek flush cache after write
- cek sector alignment
- cek concurrent access (if ada)

---

**back to:** [driver →](readme.md)

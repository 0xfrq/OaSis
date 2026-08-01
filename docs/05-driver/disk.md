# disk driver

dokumentasi ini membahas bagaimana OaSis baca dan tulis ke hard disk.

## daftar isi

- [overview](#overview)
- [ata/ide](#ataide)
- [port i/o](#port-io)
- [read sector](#read-sector)
- [write sector](#write-sector)
- [api reference](#api-reference)

---

## overview

**disk driver** di OaSis handle read/write ke ATA/IDE hard disk.

### kemampuan

- baca sector (512 bytes)
- tulis sector (512 bytes)
- detect disk presence
- PIO mode (programmed I/O)

### limitation

- belum mendukung DMA (lebih lambat dari PIO)
- belum mendukung LBA48 (max 128 GB)
- belum mendukung SATA/AHCI
- belum mendukung caching

## ata/ide

**ATA (Advanced Technology Attachment)** adalah standard untuk hard disk.

### karakteristik

- **sector size**: 512 bytes
- **addressing**: LBA (Logical Block Addressing)
- **mode**: PIO (Programmed I/O) - CPU baca/tulis langsung

### primary ata bus

OaSis pakai primary ATA bus:

```text
I/O ports: 0x1F0 - 0x1F7
IRQ: 14
```

## port i/o

ATA pakai beberapa port I/O:

| port | read | write | deskripsi |
|------|------|-------|-----------|
| 0x1F0 | data | data | data port (16-bit) |
| 0x1F1 | error | features | error/features |
| 0x1F2 | sector count | sector count | jumlah sector |
| 0x1F3 | lba low | lba low | LBA bits 0-7 |
| 0x1F4 | lba mid | lba mid | LBA bits 8-15 |
| 0x1F5 | lba high | lba high | LBA bits 16-23 |
| 0x1F6 | drive/head | drive/head | drive select + LBA bits 24-27 |
| 0x1F7 | status | command | status/command register |

### status register

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

### command register

```text
0x20: read sector (PIO)
0x30: write sector (PIO)
0xEC: identify drive
```

## read sector

### flow

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

### implementasi

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

## write sector

### flow

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

### implementasi

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

## api reference

### inisialisasi

```c
void ata_init(void);
```

inisialisasi ATA driver. detect disk presence.

### detect disk

```c
int ata_detect(void);
```

detect apakah ada disk di primary ATA.

**return:**
- `0`: disk detected
- `-1`: no disk

### read sector

```c
int ata_read_sector(uint32_t lba, void *buffer);
```

baca satu sector (512 bytes) dari disk.

**parameter:**
- `lba`: logical block address
- `buffer`: buffer untuk menyimpan data (minimal 512 bytes)

**return:**
- `0`: success
- `-1`: error

**contoh:**
```c
uint8_t buffer[512];
if (ata_read_sector(0, buffer) == 0) {
    // process data
}
```

### write sector

```c
int ata_write_sector(uint32_t lba, const void *buffer);
```

tulis satu sector (512 bytes) ke disk.

**parameter:**
- `lba`: logical block address
- `buffer`: data yang mau ditulis (512 bytes)

**return:**
- `0`: success
- `-1`: error

**contoh:**
```c
uint8_t data[512];
// fill data
if (ata_write_sector(0, data) == 0) {
    // success
}
```

### read multiple sectors

```c
int ata_read_sectors(uint32_t lba, uint32_t count, void *buffer);
```

baca multiple sectors.

**parameter:**
- `lba`: start LBA
- `count`: jumlah sector
- `buffer`: buffer (minimal count * 512 bytes)

**return:**
- `0`: success
- `-1`: error

### write multiple sectors

```c
int ata_write_sectors(uint32_t lba, uint32_t count, const void *buffer);
```

tulis multiple sectors.

**parameter:**
- `lba`: start LBA
- `count`: jumlah sector
- `buffer`: data (count * 512 bytes)

**return:**
- `0`: success
- `-1`: error

---

## helper functions

### wait ready

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

### wait drq

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

## troubleshooting

### disk tidak detected

- cek ATA controller enabled di BIOS/QEMU
- cek port 0x1F7 (harus bisa dibaca)
- cek drive select byte (0xE0 untuk drive 0)

### read/write error

- cek status register (bit 0 = error)
- cek LBA address valid
- cek buffer size (minimal 512 bytes)

### data corrupt

- cek flush cache setelah write
- cek sector alignment
- cek concurrent access (kalau ada)

---

**kembali ke:** [driver →](readme.md)

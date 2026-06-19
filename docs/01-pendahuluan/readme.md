# 01. pendahuluan

## apa itu OaSis?

OaSis adalah sistem operasi edukatif yang dirancang buat kamu yang pengen belajar gimana cara kerja OS dari nol. proyek ini bikin OS 32-bit yang jalan di arsitektur x86 (i386), lengkap dengan booting, kernel, driver, filesystem, dan shell.

**catatan:** OaSis bukan OS production-ready. ini murni buat belajar dan eksperimen.

## tujuan proyek

- **belajar fundamental OS**: ngerti gimana booting, kernel, memory management, dll
- **hands-on experience**: langsung nulis kode, bukan cuma teori
- **kode yang readable**: semua kode ditulis dengan komentar bahasa indonesia yang jelas
- **progresif**: dibangun step-by-step, dari paling dasar sampe fitur advanced

## prasyarat belajar

sebelum mulai, ada baiknya kamu udah familiar sama:

### wajib
- **bahasa c**: karena kernel ditulis pake C
- **dasar assembly x86**: buat ngerti low-level operations
- **linux command line**: buat build dan test

### recommended
- struktur data dasar (array, linked list, buffer)
- konsep operating system (process, memory, filesystem)
- git dan version control

## tools yang dibutuhin

buat development OaSis, kamu butuh:

```bash
# compiler dan assembler
gcc          # compiler C (support -m32)
nasm         # assembler

# emulator
qemu-system-i386   # buat run OS di virtual machine

# tools tambahan
grub-mkrescue      # bikin bootable ISO
xorriso            # dependency grub
make               # build system
```

## cara mulai

1. **clone repository**:
   ```bash
   git clone <repo-url>
   cd OaSis
   ```

2. **build kernel**:
   ```bash
   make
   ```

3. **run di qemu**:
   ```bash
   make run
   ```

4. **baca dokumentasi**:
   mulai dari [02-arsitektur](../02-arsitektur/readme.md) buat ngerti gambaran umum sistem

## struktur proyek

```
OaSis/
├── src/              # source code
│   ├── boot/         # bootloader
│   └── kernel/       # kernel dan driver
├── include/          # header files
├── docs/             # dokumentasi (yang lagi kamu baca)
├── Makefile          # build configuration
└── readme.md         # quick start guide
```

selanjutnya: [arsitektur →](../02-arsitektur/readme.md)

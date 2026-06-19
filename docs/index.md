---
layout: default
title: Beranda
---

# Dokumentasi OaSis

Selamat datang di dokumentasi resmi **OaSis** - sistem operasi edukatif untuk belajar!

## Apa itu OaSis?

OaSis adalah sistem operasi 32-bit untuk arsitektur x86 (i386), dibuat khusus buat belajar fundamental OS dari nol. Featuring booting, kernel, driver, filesystem, dan shell - semua dengan kode yang readable dan komentar bahasa Indonesia.

## Mulai Cepat

```bash
# Clone repository
git clone https://github.com/yourusername/OaSis.git
cd OaSis

# Build
make

# Run di QEMU
make run
```

## Struktur Dokumentasi

| Bagian | Deskripsi |
|--------|-----------|
| [01-pendahuluan](01-pendahuluan/) | Intro, tujuan, prasyarat, cara mulai |
| [02-arsitektur](02-arsitektur/) | Gambaran umum sistem, komponen utama |
| [03-booting](03-booting/) | Multiboot, entry point, inisialisasi |
| [04-kernel](04-kernel/) | Memory, interrupt, task scheduling, syscall |
| [05-driver](05-driver/) | VGA, keyboard, timer, disk, I/O port |
| [06-filesystem](06-filesystem/) | OAFS, struktur, operasi, VFS |
| [07-shell](07-shell/) | Command-line interface, daftar command |
| [08-apps](08-apps/) | Text editor, cara bikin aplikasi |
| [09-build](09-build/) | Build system, cara compile & run |

## Fitur Utama

- **Booting**: Multiboot-compliant, boot via GRUB atau QEMU
- **Kernel**: 32-bit protected mode, monolithic kernel
- **Driver**: VGA text mode, keyboard PS/2, timer PIT, ATA/IDE disk
- **Filesystem**: OAFS (Oasis File System) - inode-based custom filesystem
- **Shell**: Command-line interface dengan berbagai command
- **Text Editor**: Editor teks nano-like yang bisa edit file langsung dari shell

## Kontribusi

Ini proyek belajar! Feel free buat fork, modif, atau experiment.Kalau mau kontribusi, tinggal bikin branch, commit, dan PR.

## Lisensi

Bebas dipake buat belajar. No warranty - ini OS edukasi, bukan production-ready.

---

**Dibikin dengan kopi dan rasa penasaran.**

# dokumentasi OaSis

selamat datang di dokumentasi resmi OaSis! dokumentasi ini bakal ngebahas semua aspek dari sistem operasi OaSis, dari cara kerja booting sampe gimana bikin aplikasi di atasnya.

## daftar isi

dokumentasi ini dibagi jadi beberapa bagian utama:

### [01. pendahuluan](01-pendahuluan/readme.md)
- apa itu OaSis
- tujuan proyek
- prasyarat belajar
- cara mulai

### [02. arsitektur](02-arsitektur/readme.md)
- gambaran umum sistem
- komponen utama
- diagram alur
- keputusan desain

### [03. booting](03-booting/readme.md)
- multiboot specification
- entry point
- inisialisasi awal
- transisi ke kernel

### [04. kernel](04-kernel/readme.md)
- struktur kernel
- manajemen memori
- interrupt handling
- task scheduling
- system calls

### [05. driver](05-driver/readme.md)
- vga text mode
- keyboard ps/2
- timer pit
- ata/ide disk
- i/o port

### [06. filesystem](06-filesystem/readme.md)
- oafs (oasis file system)
- struktur disk
- inode dan data blocks
- operasi file
- virtual file system (vfs)

### [07. shell](07-shell/readme.md)
- cara kerja shell
- daftar command
- input/output
- manajemen proses

### [08. aplikasi](08-apps/readme.md)
- text editor
- cara bikin aplikasi baru
- integrasi dengan kernel

### [09. build system](09-build/readme.md)
- toolchain
- makefile
- cara compile
- cara run di qemu
- troubleshooting

## cara baca dokumentasi ini

kamu bisa baca dokumentasi ini secara berurutan dari awal sampe akhir, atau langsung loncat ke bagian yang kamu butuhin. tiap bagian dirancang supaya bisa berdiri sendiri, tapi ada referensi ke bagian lain kalo perlu.

## konvensi penulisan

- kode contoh pake monospace font
- perintah shell diawali dengan `$`
- output perintah ditunjukin tanpa prefix
- catatan penting ditandai dengan **catatan:**

## kontribusi

kalo kamu nemu typo, error, atau ada yang kurang jelas di dokumentasi ini, feel free buat bikin issue atau pull request!


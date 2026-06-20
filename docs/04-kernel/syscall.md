# system calls

dokumentasi ini ngebahas gimana aplikasi di OaSis bisa minta service dari kernel.

## daftar isi

- [apa itu system call](#apa-itu-system-call)
- [mekanisme](#mekanisme)
- [syscall table](#syscall-table)
- [implementasi](#implementasi)
- [contoh penggunaan](#contoh-penggunaan)

---

## apa itu system call

**system call** adalah cara aplikasi minta kernel buat ngelakuin sesuatu yang butuh privilege.

### kenapa butuh system call?

aplikasi jalan di user mode (kalo ada), gak bisa langsung akses hardware atau memory kernel. jadi aplikasi harus minta kernel via system call.

**catatan:** OaSis belum punya user mode, tapi system call tetep dipake buat consistency dan future-proofing.

### contoh system call

- `write()` - tulis ke file/screen
- `read()` - baca dari file/keyboard
- `open()` - buka file
- `close()` - tutup file
- `exit()` - keluar dari process

## mekanisme

OaSis pake **interrupt 0x80** buat system call.

### flow

```
aplikasi
  ↓
setup parameters di registers
  ↓
int 0x80
  ↓
cpu switch ke kernel mode
  ↓
kernel syscall handler
  ↓
kernel proses request
  ↓
kernel return result
  ↓
aplikasi continue
```

### parameter passing

parameter di-pass via registers:

```asm
mov eax, syscall_number  ; nomor syscall
mov ebx, param1          ; parameter 1
mov ecx, param2          ; parameter 2
mov edx, param3          ; parameter 3
int 0x80                 ; trigger syscall
; result di eax
```

### contoh: write syscall

```asm
; write(fd, buffer, length)
mov eax, 1          ; syscall number (write)
mov ebx, 1          ; fd (1 = stdout)
mov ecx, message    ; buffer pointer
mov edx, 13         ; length
int 0x80            ; call kernel
; eax = bytes written
```

## syscall table

daftar system call yang di-support OaSis:

### i/o syscalls

| no | nama | parameter | return | deskripsi |
|----|------|-----------|--------|-----------|
| 1 | `write` | ebx=fd, ecx=buf, edx=len | bytes written | tulis ke file/fd |
| 2 | `read` | ebx=fd, ecx=buf, edx=len | bytes read | baca dari file/fd |
| 3 | `open` | ebx=path, ecx=flags | fd | buka file |
| 4 | `close` | ebx=fd | 0/-1 | tutup file |
| 5 | `seek` | ebx=fd, ecx=offset, edx=whence | new offset | move file position |

### process syscalls

| no | nama | parameter | return | deskripsi |
|----|------|-----------|--------|-----------|
| 10 | `exit` | ebx=status | - | keluar dari task |
| 11 | `fork` | - | pid | duplicate task |
| 12 | `getpid` | - | pid | dapetin task id |
| 13 | `wait` | ebx=pid | status | tunggu task selesai |
| 14 | `yield` | - | 0 | give up cpu |
| 15 | `sleep` | ebx=ticks | 0 | sleep for ticks |

### memory syscalls (planned)

| no | nama | parameter | return | deskripsi |
|----|------|-----------|--------|-----------|
| 20 | `mmap` | ebx=addr, ecx=len | ptr | map memory |
| 21 | `munmap` | ebx=addr, ecx=len | 0/-1 | unmap memory |

## implementasi

### syscall handler

```c
void syscall_handler(registers_t *regs) {
    uint32_t syscall_num = regs->eax;
    uint32_t result = 0;
    
    switch (syscall_num) {
        case 1:  // write
            result = sys_write(regs->ebx, regs->ecx, regs->edx);
            break;
            
        case 2:  // read
            result = sys_read(regs->ebx, regs->ecx, regs->edx);
            break;
            
        case 3:  // open
            result = sys_open(regs->ebx, regs->ecx);
            break;
            
        case 4:  // close
            result = sys_close(regs->ebx);
            break;
            
        case 10: // exit
            sys_exit(regs->ebx);
            break;
            
        case 14: // yield
            task_yield();
            result = 0;
            break;
            
        case 15: // sleep
            task_sleep(regs->ebx);
            result = 0;
            break;
            
        default:
            // unknown syscall
            result = -1;
            break;
    }
    
    // return result via eax
    regs->eax = result;
}
```

### syscall implementation

#### write

```c
uint32_t sys_write(uint32_t fd, const char *buf, uint32_t len) {
    if (fd == 1) {
        // stdout
        for (uint32_t i = 0; i < len; i++) {
            vga_putchar(buf[i]);
        }
        return len;
    } else {
        // file
        return vfs_write(fd, buf, len);
    }
}
```

#### read

```c
uint32_t sys_read(uint32_t fd, char *buf, uint32_t len) {
    if (fd == 0) {
        // stdin
        return keyboard_read(buf, len);
    } else {
        // file
        return vfs_read(fd, buf, len);
    }
}
```

#### open

```c
uint32_t sys_open(const char *path, uint32_t flags) {
    return vfs_open(path, flags);
}
```

#### close

```c
uint32_t sys_close(uint32_t fd) {
    return vfs_close(fd);
}
```

## contoh penggunaan

### contoh 1: write ke stdout

```c
void task_example(void) {
    const char *msg = "hello world\n";
    uint32_t len = 12;
    
    // inline assembly
    asm volatile(
        "mov $1, %%eax\n"      // syscall: write
        "mov $1, %%ebx\n"      // fd: stdout
        "mov %0, %%ecx\n"      // buffer
        "mov %1, %%edx\n"      // length
        "int $0x80\n"          // trigger syscall
        :
        : "r"(msg), "r"(len)
        : "eax", "ebx", "ecx", "edx"
    );
}
```

### contoh 2: c wrapper

```c
// wrapper function
int32_t write(int fd, const char *buf, int len) {
    int32_t result;
    asm volatile(
        "mov $1, %%eax\n"
        "mov %1, %%ebx\n"
        "mov %2, %%ecx\n"
        "mov %3, %%edx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(fd), "r"(buf), "r"(len)
        : "eax", "ebx", "ecx", "edx"
    );
    return result;
}

// usage
void task_example(void) {
    write(1, "hello\n", 6);
}
```

### contoh 3: sleep

```c
void task_example(void) {
    write(1, "sleeping...\n", 12);
    
    // sleep 1 second (100 ticks * 10ms)
    asm volatile(
        "mov $15, %%eax\n"
        "mov $100, %%ebx\n"
        "int $0x80\n"
        :
        :
        : "eax", "ebx"
    );
    
    write(1, "awake!\n", 7);
}
```

---

## testing syscall

buat test syscall, kamu bisa bikin task yang call syscall:

```c
void test_task(void) {
    const char *msg = "testing syscall\n";
    
    // call write syscall
    asm volatile(
        "mov $1, %%eax\n"
        "mov $1, %%ebx\n"
        "mov %0, %%ecx\n"
        "mov $16, %%edx\n"
        "int $0x80\n"
        :
        : "r"(msg)
        : "eax", "ebx", "ecx", "edx"
    );
    
    task_exit();
}
```

---

**kembali ke:** [kernel →](readme.md)

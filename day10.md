# Day 10: I/O Subsystem

## Overview

Day 10 implements the **I/O Subsystem** for Oasis OS, providing a Unix-like file descriptor interface for all I/O operations. This is a fundamental abstraction that allows programs to read and write data using a uniform interface.

## Features Implemented

### 1. File Descriptor Table
- Per-process file descriptor table with 16 slots
- Standard file descriptors: stdin (0), stdout (1), stderr (2)
- Automatic inheritance on fork()
- Proper reference counting for shared descriptors

### 2. Standard I/O
- **stdin (fd 0)**: Console input with keyboard reading
- **stdout (fd 1)**: Console output to VGA
- **stderr (fd 2)**: Error output to VGA

### 3. Pipe Support
- Circular buffer pipes (512 bytes)
- Up to 8 system-wide pipes
- Read/write blocking semantics
- EOF detection when writers close
- Broken pipe detection when readers close

### 4. File Descriptor Operations
- `open()`: Open files/devices (console devices supported)
- `close()`: Close file descriptors
- `read()`: Read from file descriptors
- `write()`: Write to file descriptors
- `dup()`: Duplicate file descriptor to lowest available
- `dup2()`: Duplicate to specific file descriptor
- `seek()`: Seek in files (placeholder for Day 11-12)

## New System Calls

| Syscall | Number | Description |
|---------|--------|-------------|
| `SYSCALL_OPEN` | 9 | Open a file/device |
| `SYSCALL_CLOSE` | 10 | Close a file descriptor |
| `SYSCALL_READ` | 11 | Read from fd |
| `SYSCALL_WRITE_FD` | 12 | Write to fd |
| `SYSCALL_PIPE` | 13 | Create a pipe |
| `SYSCALL_DUP` | 14 | Duplicate fd |
| `SYSCALL_DUP2` | 15 | Duplicate to specific fd |
| `SYSCALL_SEEK` | 16 | Seek in file |
| `SYSCALL_FDINFO` | 17 | Debug: print fd table |
| `SYSCALL_GETPPID` | 8 | Get parent PID |

## New Files

- `kernel/fd.h` - File descriptor definitions and prototypes
- `kernel/fd.c` - File descriptor implementation
- `kernel/tasks_io.h` - I/O demo task prototypes
- `kernel/tasks_io.c` - I/O demo task implementations

## Modified Files

- `kernel/syscall.h` - Added new syscall numbers and wrappers
- `kernel/syscall.c` - Added syscall handlers
- `kernel/task.h` - Added fd_table pointer to task struct
- `kernel/task.c` - Initialize/copy fd tables on create/fork
- `kernel/kernel.c` - Added fd_init() and shell commands
- `Makefile` - Added fd.c and tasks_io.c to build

## Testing

### Shell Commands

```
oasis> help          # Shows all commands including new I/O commands
oasis> iotest        # Run complete I/O subsystem test
oasis> fdinfo        # Show file descriptor table
oasis> pipetest      # Test pipe communication
```

### Expected Output from `iotest`

```
╔════════════════════════════════════════╗
║       Day 10: I/O Subsystem Test       ║
╚════════════════════════════════════════╝

[1/5] Initial file descriptor state:
File Descriptor Table:
  fd 0: CONSOLE flags=R ref=1
  fd 1: CONSOLE flags=W ref=1
  fd 2: CONSOLE flags=W ref=1

[2/5] Testing stdout/stderr...
[stdout] Standard output works!
[stderr] Standard error works!

[3/5] Testing pipes...
  Pipe created: [3,4]
  Data through pipe: 'PIPE_TEST_DATA'
  Pipe closed successfully

[4/5] Testing fd duplication...
  Duplicated stdout to fd 3

[5/5] Final file descriptor state:
File Descriptor Table:
  fd 0: CONSOLE flags=R ref=1
  fd 1: CONSOLE flags=W ref=1
  fd 2: CONSOLE flags=W ref=1

╔════════════════════════════════════════╗
║         I/O Subsystem: PASSED          ║
╚════════════════════════════════════════╝
```

## Architecture

```
┌─────────────────────────────────────────┐
│           User Programs                  │
│   sys_read() sys_write_fd() sys_pipe()  │
├─────────────────────────────────────────┤
│         System Call Layer (int 0x80)     │
│   syscall_read() syscall_write_fd()     │
├─────────────────────────────────────────┤
│           FD Layer (fd.c)               │
│   fd_read() fd_write() fd_pipe()        │
├───────────────┬─────────────────────────┤
│  Console I/O  │     Pipe Buffer         │
│  (keyboard,   │   (circular buffer,     │
│   VGA)        │    512 bytes)           │
├───────────────┴─────────────────────────┤
│           Hardware Layer                 │
│   keyboard.c, vga.c                     │
└─────────────────────────────────────────┘
```

## Data Structures

### fd_entry_t
```c
typedef struct fd_entry {
    fd_type_t type;         // NONE, CONSOLE, PIPE_READ, PIPE_WRITE, FILE
    uint32_t flags;         // FD_FLAG_READ, FD_FLAG_WRITE, etc.
    uint32_t ref_count;     // For sharing between processes
    uint32_t offset;        // File position (for files)
    union {
        pipe_t *pipe;       // For pipe types
        uint32_t device_id; // For device types
    } data;
} fd_entry_t;
```

### pipe_t
```c
typedef struct pipe {
    uint8_t buffer[512];    // Circular buffer
    uint32_t read_pos;      // Read position
    uint32_t write_pos;     // Write position
    uint32_t count;         // Bytes in buffer
    uint32_t readers;       // Number of read ends
    uint32_t writers;       // Number of write ends
    uint8_t active;         // Is pipe in use?
} pipe_t;
```

## Usage Examples

### Writing to stdout
```c
sys_write_fd(STDOUT_FILENO, "Hello World!\n", 13);
```

### Reading from stdin
```c
char buf[64];
int n = sys_read(STDIN_FILENO, buf, 63);
buf[n] = '\0';
```

### Creating and using a pipe
```c
int pipefd[2];
sys_pipe(pipefd);  // pipefd[0]=read, pipefd[1]=write

sys_write_fd(pipefd[1], "data", 4);

char buf[16];
sys_read(pipefd[0], buf, 16);

sys_close(pipefd[0]);
sys_close(pipefd[1]);
```

### Duplicating file descriptors
```c
int backup = sys_dup(STDOUT_FILENO);  // Backup stdout
sys_dup2(backup, STDOUT_FILENO);      // Restore stdout
sys_close(backup);
```

## Limitations

1. **No filesystem yet** - Only console devices supported, files will come in Day 11-12
2. **Blocking I/O only** - No non-blocking mode yet
3. **No select/poll** - Single-threaded I/O only
4. **Static pipe allocation** - Fixed 8 pipes maximum
5. **No TTY control** - Raw keyboard input only

## Building

```bash
make clean
make
make run
```

## Next Steps (Day 11-12)

- Block device driver (ATA disk)
- Simple filesystem implementation
- File read/write from disk
- Directory traversal

## Proof of Completion

- ✅ File descriptor table per process
- ✅ Open, read, write, close operations
- ✅ Standard I/O (stdin, stdout, stderr)
- ✅ Pipe basics working
- ✅ Programs can read/write data

Day 10 complete! The I/O subsystem provides the foundation for all future file and device operations.

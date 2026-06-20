---
layout: default
title: Shell
---

# 07. Shell

## Command Interface

The shell runs as an infinite loop in `kernel_main()`:

```
while (1) {
    c = keyboard_getchar();
    if (c == '\n') { execute_command(input); index = 0; }
    else { input[index++] = c; echo; }
}
```

## Available Commands

### File Operations
| Command | Description |
|---------|-------------|
| `ls [path]` | List directory (color-coded: yellow=dir, white=file) |
| `cd <path>` | Change directory (supports ..) |
| `pwd` | Print working directory |
| `mkdir <path>` | Create directory |
| `touch <file>` | Create empty file |
| `rm <file>` | Remove file |
| `cat <file>` | Display file contents |
| `write <file> <text>` | Write text to file (overwrites) |
| `append <file> <text>` | Append text to file |
| `hexdump <file>` | Show file in hex |

### Development
| Command | Description |
|---------|-------------|
| `edit <file>` | Text editor (nano-like) |
| `nasm <file>` | Assemble and run .asm (ring 0) |
| `user <file>` | Assemble and run .asm (ring 3) |
| `occ <file>` | Compile and run .c |

### System
| Command | Description |
|---------|-------------|
| `help` | Show commands |
| `clear` | Clear screen |
| `uptime` | System uptime |
| `meminfo` | Physical memory info |
| `dmesg` | Kernel log |
| `syscall` | List all syscalls |
| `echo <text>` | Print text |

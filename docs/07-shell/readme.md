# shell

dokumentasi ini ngebahas command-line interface di OaSis.

## daftar isi

- [apa itu shell](#apa-itu-shell)
- [cara kerja shell](#cara-kerja-shell)
- [command list](#command-list)
- [input/output](#inputoutput)
- [environment](#environment)

---

## apa itu shell

**shell** adalah program yang jadi interface antara user dan kernel. shell baca command dari user dan execute.

### fungsi shell

- **read input**: baca command dari keyboard
- **parse command**: pisahkan command dan arguments
- **execute**: jalankan command (built-in atau external)
- **display result**: tampilkan output ke user

### jenis command

**built-in commands**: command yang di-implement langsung di shell
- `help`, `clear`, `echo`, `cd`, `pwd`, `exit`

**system commands**: command yang call kernel function
- `ls`, `cat`, `write`, `append`, `mkdir`, `rmdir`, `rm`, `touch`

**applications**: program yang jalan di OaSis
- `edit` (text editor)

## cara kerja shell

### main loop

```c
void shell_run(void) {
    char input[256];
    
    while (1) {
        // display prompt
        print_prompt();
        
        // read input
        shell_readline(input, sizeof(input));
        
        // skip empty input
        if (strlen(input) == 0) continue;
        
        // parse command
        char *cmd = strtok(input, " ");
        char *args = strtok(NULL, "");
        
        // execute command
        shell_execute(cmd, args);
    }
}
```

### parsing

```c
void shell_parse(char *input, char **cmd, char **args) {
    // skip leading spaces
    while (*input == ' ') input++;
    
    // find command (first word)
    *cmd = input;
    
    // find end of command
    while (*input && *input != ' ') input++;
    
    if (*input) {
        *input = '\0';  // terminate command
        input++;
        
        // skip spaces before args
        while (*input == ' ') input++;
        *args = input;
    } else {
        *args = NULL;
    }
}
```

### execute

```c
void shell_execute(const char *cmd, const char *args) {
    // built-in commands
    if (strcmp(cmd, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd, "clear") == 0) {
        vga_clear();
    } else if (strcmp(cmd, "echo") == 0) {
        cmd_echo(args);
    } else if (strcmp(cmd, "exit") == 0) {
        cmd_exit();
    }
    // filesystem commands
    else if (strcmp(cmd, "ls") == 0) {
        cmd_ls(args);
    } else if (strcmp(cmd, "cat") == 0) {
        cmd_cat(args);
    } else if (strcmp(cmd, "write") == 0) {
        cmd_write(args);
    } else if (strcmp(cmd, "mkdir") == 0) {
        cmd_mkdir(args);
    } else if (strcmp(cmd, "rm") == 0) {
        cmd_rm(args);
    } else if (strcmp(cmd, "touch") == 0) {
        cmd_touch(args);
    } else if (strcmp(cmd, "edit") == 0) {
        cmd_edit(args);
    }
    // unknown command
    else {
        vga_puts("unknown command: ");
        vga_puts(cmd);
        vga_puts("\n");
    }
}
```

## command list

### help

```
help
```

tampilkan daftar command yang available.

**output:**
```
available commands:
  help    - show this message
  clear   - clear screen
  echo    - print text
  exit    - exit shell
  ls      - list directory
  cat     - display file content
  write   - write to file
  append  - append to file
  mkdir   - create directory
  rmdir   - remove directory
  rm      - remove file
  touch   - create empty file
  edit    - text editor
```

### clear

```
clear
```

bersihin screen.

### echo

```
echo [text]
```

print text ke screen.

**contoh:**
```
> echo hello world
hello world
```

### exit

```
exit
```

keluar dari shell (shutdown system).

### ls

```
ls [path]
```

list isi directory.

**parameter:**
- `path`: directory path (default: current directory)

**contoh:**
```
> ls
.          dir
..         dir
file.txt   file

> ls /home
user       dir
documents  dir
```

### cat

```
cat <path>
```

tampilkan isi file.

**parameter:**
- `path`: file path

**contoh:**
```
> cat /file.txt
hello world
this is a test
```

### write

```
write <path> <content>
```

tulis content ke file (overwrite).

**parameter:**
- `path`: file path
- `content`: text yang mau ditulis

**contoh:**
```
> write /test.txt hello world
wrote 11 bytes to /test.txt
```

### append

```
append <path> <content>
```

tambah content ke ujung file.

**parameter:**
- `path`: file path
- `content`: text yang mau ditambah

**contoh:**
```
> append /test.txt more text
appended 9 bytes to /test.txt
```

### mkdir

```
mkdir <path>
```

bikin directory baru.

**parameter:**
- `path`: directory path

**contoh:**
```
> mkdir /newdir
created directory /newdir
```

### rmdir

```
rmdir <path>
```

hapus directory (harus kosong).

**parameter:**
- `path`: directory path

**contoh:**
```
> rmdir /emptydir
removed directory /emptydir
```

### rm

```
rm <path>
```

hapus file.

**parameter:**
- `path`: file path

**contoh:**
```
> rm /file.txt
removed /file.txt
```

### touch

```
touch <path>
```

bikin file kosong.

**parameter:**
- `path`: file path

**contoh:**
```
> touch /newfile.txt
created /newfile.txt
```

### edit

```
edit <path>
```

buka text editor.

**parameter:**
- `path`: file path

**kontrol:**
- arrow keys: navigasi
- ctrl+s: save
- ctrl+x: exit
- backspace: hapus karakter
- enter: newline

**contoh:**
```
> edit /document.txt
[opens text editor]
```

## input/output

### input handling

shell baca input karakter per karakter:

```c
void shell_readline(char *buf, int max_len) {
    int i = 0;
    
    while (i < max_len - 1) {
        char c = keyboard_getchar();
        
        if (c == '\n') {
            // enter - selesai
            vga_putchar('\n');
            break;
        } else if (c == '\b') {
            // backspace
            if (i > 0) {
                i--;
                vga_putchar('\b');
            }
        } else if (c >= 32 && c < 127) {
            // printable character
            buf[i++] = c;
            vga_putchar(c);
        }
    }
    
    buf[i] = '\0';
}
```

### output

shell pake vga driver buat display:

```c
// print string
vga_puts("hello world\n");

// print character
vga_putchar('A');

// print formatted
char buf[50];
sprintf(buf, "size: %d bytes\n", size);
vga_puts(buf);
```

## environment

### current directory

shell track current working directory:

```c
char cwd[256] = "/";

void cmd_cd(const char *path) {
    if (vfs_chdir(path) == 0) {
        strncpy(cwd, path, 256);
    } else {
        vga_puts("cd: directory not found\n");
    }
}

void cmd_pwd(void) {
    vga_puts(cwd);
    vga_puts("\n");
}
```

### prompt

shell display prompt dengan current directory:

```c
void print_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("oasis");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts(":");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    vga_puts(cwd);
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("$ ");
}
```

**contoh prompt:**
```
oasis:/home$ 
oasis:/home/user$ 
oasis:/$ 
```

---

## contoh session

```
oasis:/$ help
available commands:
  help    - show this message
  clear   - clear screen
  echo    - print text
  exit    - exit shell
  ls      - list directory
  cat     - display file content
  write   - write to file
  append  - append to file
  mkdir   - create directory
  rmdir   - remove directory
  rm      - remove file
  touch   - create empty file
  edit    - text editor

oasis:/$ mkdir /home
created directory /home

oasis:/$ ls
.          dir
..         dir
home       dir

oasis:/$ touch /home/test.txt
created /home/test.txt

oasis:/$ write /home/test.txt hello world
wrote 11 bytes to /home/test.txt

oasis:/$ cat /home/test.txt
hello world

oasis:/$ append /home/test.txt more text
appended 9 bytes to /home/test.txt

oasis:/$ cat /home/test.txt
hello world more text

oasis:/$ rm /home/test.txt
removed /home/test.txt

oasis:/$ exit
shutting down...
```

---

**kembali ke:** [dokumentasi →](../readme.md) | **selanjutnya:** [apps →](../08-apps/readme.md)

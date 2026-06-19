#include "asm.h"
#include "vga.h"
#include "keyboard.h"
#include "string.h"
#include "pmm.h"
#include "paging.h"
#include "vfs.h"
#include "io.h"
#include "klibc.h"

/* debug port QEMU (0xE9 hack), bisa di-capture lewat -debugcon */
static void dbg_putc(char c) {
    outb(0xE9, (uint8_t)c);
}
static void dbg_print(const char *s) {
    while (*s) dbg_putc(*s++);
}
static void dbg_hex(uint32_t v) {
    char buf[16];
    itoa((int)v, buf, 16);
    dbg_print(buf);
}
static void dbg_dec(int v) {
    char buf[16];
    itoa(v, buf, 10);
    dbg_print(buf);
}

/* virtual address buat kode hasil assemble */
#define CODE_VIRT  0x40000000
#define CODE_SIZE  16384

/* batas label dan patch */
#define MAX_LABELS  32
#define MAX_PATCHES 128
#define MAX_LINE_LEN 512

/* ====== External symbol table ======
 * Tabel fungsi C yang bisa dipanggil dari program yang di-assemble.
 * Nama label -> alamat fungsi di kernel.
 * Kalau assembler gak nemu label di labels[], dia cari di sini.
 */
typedef struct {
    const char *name;    /* nama label (e.g. "_printf") */
    void       *addr;    /* alamat fungsi di kernel */
} extern_sym_t;

static const extern_sym_t extern_syms[] = {
    /* stdio - output */
    { "_printf",   (void *)klibc_printf   },
    { "_putchar",  (void *)klibc_putchar  },
    { "_puts",     (void *)klibc_puts     },
    { "_sprintf",  (void *)klibc_sprintf  },

    /* stdio - input */
    { "_scanf",    (void *)klibc_scanf    },
    { "_getchar",  (void *)klibc_getchar  },
    { "_gets",     (void *)klibc_gets     },

    /* stdlib */
    { "_atoi",     (void *)klibc_atoi     },

    /* memory */
    { "_malloc",   (void *)klibc_malloc   },
    { "_free",     (void *)klibc_free     },
    { "_calloc",   (void *)klibc_calloc   },
    { "_realloc",  (void *)klibc_realloc  },

    /* kernel VGA (bonus, biar bisa dipanggil langsung) */
    { "_vga_print", (void *)vga_print     },
    { "_vga_putc",  (void *)vga_putc      },
    { "_vga_clear", (void *)vga_clear     },

    { 0, 0 }  /* sentinel */
};

/* nama register 32-bit, urutan sesuai nomor register x86 */
static const char *reg_names[] = {
    "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi", 0
};

/* nama segment register */
static const char *seg_names[] = { "es", "cs", "ss", "ds", "fs", "gs", 0 };

/* forward decl */
static int streq(const char *a, const char *b);

/* cek apakah string adalah segment register, return 1 kalo iya */
static int is_seg_reg(const char *s) {
    for (int i = 0; seg_names[i]; i++)
        if (streq(s, seg_names[i])) return 1;
    return 0;
}

/* buffer output kode mesin */
static uint8_t code_buf[CODE_SIZE];
static int code_len;

typedef struct {
    uint8_t modrm;      /* mod + rm part */
    uint8_t has_sib;
    uint8_t sib;
    uint8_t disp_size;  /* 0, 1, or 4 */
    uint32_t disp;
} mem_op_t;

/* tabel label: nama -> posisi byte di code_buf */
static struct {
    char name[32];
    int  pos;
} labels[MAX_LABELS];
static int num_labels;

/* patch: lokasi di code_buf yang perlu diisi alamat label nanti */
static struct {
    int  pos;        /* byte pertama yang perlu di-patch */
    int  from;       /* posisi setelah instruksi (buat hitung relatif) */
    char target[32]; /* nama label tujuan */
    int  type;       /* 0 = rel8, 1 = rel32, 2 = abs32 (buat mov reg, label) */
} patches[MAX_PATCHES];
static int num_patches;

/* emit 1 byte ke buffer */
static void emit(uint8_t b) {
    if (code_len < CODE_SIZE)
        code_buf[code_len++] = b;
}

/* emit 4 byte little-endian */
static void emit32(uint32_t v) {
    emit(v & 0xFF);
    emit((v >> 8) & 0xFF);
    emit((v >> 16) & 0xFF);
    emit((v >> 24) & 0xFF);
}

/* bandingin dua string, return 1 kalo sama */
static int streq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

/* copy string dengan batas ukuran */
static void strncopy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* cek awalan string */
static int starts_with(const char *s, const char *prefix) {
    while (*prefix) {
        if (*s++ != *prefix++) return 0;
    }
    return 1;
}

/* hapus spasi di kiri dan kanan string */
static void trim(char *s) {
    int i = 0;
    while (s[i] == ' ' || s[i] == '\t') i++;
    if (i > 0) {
        int j = 0;
        while (s[i]) s[j++] = s[i++];
        s[j] = 0;
    }
    int len = (int)strlen(s);
    while (len > 0 && (s[len-1] == ' ' || s[len-1] == '\t' || s[len-1] == '\r'))
        s[--len] = 0;
}

/* ubah huruf ke lowercase */
static char to_lower(char c) {
    if (c >= 'A' && c <= 'Z') return (char)(c - 'A' + 'a');
    return c;
}

/* lowercase semua huruf di string */
static void str_lower(char *s) {
    while (*s) { *s = to_lower(*s); s++; }
}

/* parse integer desimal atau hex (prefix 0x), return 1 kalo sukses */
static int parse_int(const char *s, uint32_t *out) {
    if (!s || !*s) return 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        uint32_t v = 0;
        s += 2;
        if (!*s) return 0;
        while (*s) {
            char c = to_lower(*s);
            if (c >= '0' && c <= '9')      v = v * 16 + (uint32_t)(c - '0');
            else if (c >= 'a' && c <= 'f') v = v * 16 + (uint32_t)(c - 'a' + 10);
            else return 0;
            s++;
        }
        *out = v; return 1;
    }
    uint32_t v = 0;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    if (!*s) return 0;
    while (*s) {
        if (*s < '0' || *s > '9') return 0;
        v = v * 10 + (uint32_t)(*s - '0');
        s++;
    }
    *out = neg ? (uint32_t)(-(int32_t)v) : v;
    return 1;
}

/* cari nomor register (0-7), return -1 kalo bukan register
 * Juga kenali alias 16-bit dan 8-bit (ax->eax, al->eax, dll) */
static int parse_reg(const char *s) {
    /* coba 32-bit dulu */
    for (int i = 0; reg_names[i]; i++)
        if (streq(s, reg_names[i])) return i;
    /* coba 16-bit alias */
    struct { const char *n16; int idx; } alias16[] = {
        {"ax",0}, {"cx",1}, {"dx",2}, {"bx",3},
        {"sp",4}, {"bp",5}, {"si",6}, {"di",7}, {0,0}
    };
    for (int i = 0; alias16[i].n16; i++)
        if (streq(s, alias16[i].n16)) return alias16[i].idx;
    /* coba 8-bit alias */
    struct { const char *n8; int idx; } alias8[] = {
        {"al",0}, {"cl",1}, {"dl",2}, {"bl",3},
        {"ah",0}, {"ch",1}, {"dh",2}, {"bh",3}, {0,0}
    };
    for (int i = 0; alias8[i].n8; i++)
        if (streq(s, alias8[i].n8)) return alias8[i].idx;
    return -1;
}

/* cari posisi label di code_buf, return -1 kalo belum ada */
static int find_label(const char *name) {
    for (int i = 0; i < num_labels; i++)
        if (streq(labels[i].name, name)) return labels[i].pos;
    return -1;
}

/* cari alamat extern symbol, return 0 kalo gak ketemu */
static uint32_t find_extern(const char *name) {
    for (int i = 0; extern_syms[i].name; i++) {
        if (streq(extern_syms[i].name, name))
            return (uint32_t)extern_syms[i].addr;
    }
    return 0;
}

/* daftar patch yang perlu di-resolve nanti */
static void add_patch(int pos, int from, const char *target, int type) {
    if (num_patches >= MAX_PATCHES) return;
    patches[num_patches].pos  = pos;
    patches[num_patches].from = from;
    strncopy(patches[num_patches].target, target, 32);
    patches[num_patches].type = type;
    num_patches++;
}

/* bangun byte ModR/M untuk operand register-register */
static uint8_t modrm_rr(int reg, int rm) {
    return (uint8_t)(0xC0 | ((reg & 7) << 3) | (rm & 7));
}

/* pisah "dst, src" jadi dua string, return 1 kalo ada koma */
static int split_ops(char *ops, char *dst, char *src) {
    int i = 0;
    while (ops[i] && ops[i] != ',') i++;
    if (!ops[i]) return 0;
    strncopy(dst, ops, i + 1);
    dst[i] = 0; trim(dst);
    /* prevent overflow of 32-byte src buffer */
    strncopy(src, ops + i + 1, 32);
    trim(src);
    return 1;
}

/* emit memory operand part of an instruction */
static void emit_mem(uint8_t reg_field, mem_op_t *mem) {
    emit((uint8_t)(mem->modrm | ((reg_field & 7) << 3)));
    if (mem->has_sib) emit(mem->sib);
    if (mem->disp_size == 1) emit((uint8_t)(mem->disp & 0xFF));
    else if (mem->disp_size == 4) emit32(mem->disp);
}

/* parse operand memory sederhana, misalnya "[0xB8000]" atau "[eax]" atau "[ebp-4]".
 * modrm_out diisi byte ModRM, imm_out diisi offset/address kalo ada, has_imm diisi 1 kalo imm_out valid.
 */
static int parse_mem(const char *s, mem_op_t *out) {
    if (s[0] != '[') return 0;
    int len = (int)strlen(s);
    if (s[len-1] != ']') return 0;

    char inner[32];
    strncopy(inner, s + 1, len - 1);
    inner[len - 2] = 0;
    trim(inner);

    out->has_sib = 0;
    out->disp_size = 0;
    out->disp = 0;

    /* absolute address [0x1234] */
    uint32_t addr;
    if (parse_int(inner, &addr)) {
        out->modrm = 0x05; /* Mod=00, R/M=101 (disp32) */
        out->disp = addr;
        out->disp_size = 4;
        return 1;
    }

    /* register with optional offset: [reg+offset] or [reg-offset] */
    char reg_name[16];
    int plus_pos = -1, minus_pos = -1;
    for (int i = 0; inner[i]; i++) {
        if (inner[i] == '+') plus_pos = i;
        if (inner[i] == '-') minus_pos = i;
    }

    int reg_idx = -1;
    int32_t offset = 0;

    if (plus_pos >= 0) {
        strncopy(reg_name, inner, plus_pos + 1);
        reg_name[plus_pos] = 0; trim(reg_name);
        reg_idx = parse_reg(reg_name);
        parse_int(inner + plus_pos + 1, (uint32_t *)&offset);
    } else if (minus_pos >= 0) {
        strncopy(reg_name, inner, minus_pos + 1);
        reg_name[minus_pos] = 0; trim(reg_name);
        reg_idx = parse_reg(reg_name);
        uint32_t val;
        parse_int(inner + minus_pos + 1, &val);
        offset = -(int32_t)val;
    } else {
        reg_idx = parse_reg(inner);
    }

    if (reg_idx < 0) return 0;

    /* handle ESP (register 4) - always needs SIB */
    if (reg_idx == 4) {
        out->has_sib = 1;
        out->sib = 0x24; /* Scale=0, Index=none, Base=ESP */
        out->modrm = 0x04; /* R/M=100 (SIB) */
    } else {
        out->modrm = (uint8_t)reg_idx;
    }

    if (offset == 0 && reg_idx != 5) {
        /* Mod=00, unless it's EBP which needs disp8=0 */
        out->modrm |= 0x00;
    } else if (offset >= -128 && offset <= 127) {
        out->modrm |= 0x40; /* Mod=01 */
        out->disp = (uint32_t)offset;
        out->disp_size = 1;
    } else {
        out->modrm |= 0x80; /* Mod=10 */
        out->disp = (uint32_t)offset;
        out->disp_size = 4;
    }

    return 1;
}

/* generate: mov */
static int gen_mov(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst);  /* keep src case for labels */
    int rd = parse_reg(dst), rs = parse_reg(src);

    mem_op_t mem;

    /* 1. mov r32, r32 */
    if (rd >= 0 && rs >= 0) {
        emit(0x89); emit(modrm_rr(rs, rd));
        return 0;
    }

    /* 2. mov r32, [mem] */
    if (rd >= 0 && parse_mem(src, &mem)) {
        emit(0x8B);
        emit_mem((uint8_t)rd, &mem);
        return 0;
    }

    /* 3. mov [mem], r32 */
    if (rs >= 0 && parse_mem(dst, &mem)) {
        emit(0x89);
        emit_mem((uint8_t)rs, &mem);
        return 0;
    }

    /* 4. mov dword [mem], imm32 */
    if (streq(dst, "dword") || starts_with(dst, "dword ")) {
        char *mem_part = dst + 5;
        trim(mem_part);
        if (parse_mem(mem_part, &mem)) {
            uint32_t imm;
            if (parse_int(src, &imm)) {
                emit(0xC7);
                emit_mem(0, &mem);
                emit32(imm);
                return 0;
            }
        }
    }

    /* 5. mov r32, imm32 atau label */
    if (rd >= 0) {
        uint32_t imm;
        emit((uint8_t)(0xB8 + rd));
        if (parse_int(src, &imm)) {
            emit32(imm);
        } else {
            /* anggap sebagai label */
            int target = find_label(src);
            if (target >= 0) {
                emit32(CODE_VIRT + target);
            } else {
                add_patch(code_len, 0, src, 2); /* type 2 = abs32 */
                emit32(0);
            }
        }
        return 0;
    }

    /* 6. mov byte [mem], imm8 (kasus khusus buat nulis char ke vga misal) */
    if (streq(dst, "byte") || starts_with(dst, "byte ")) {
        /* skip word 'byte' */
        char *mem_part = dst + 4;
        trim(mem_part);
        if (parse_mem(mem_part, &mem)) {
            uint32_t imm;
            if (parse_int(src, &imm)) {
                emit(0xC6);
                emit_mem(0, &mem);
                emit((uint8_t)(imm & 0xFF));
                return 0;
            }
        }
    }

    /* 7. mov seg_reg, r16 (e.g. mov ds, ax) */
    if (is_seg_reg(dst) && rs >= 0) {
        /* 8E /r: mov seg_reg, r/m16 */
        static const char seg_codes[] = { 0, 1, 2, 3, 4, 5 }; /* es,cs,ss,ds,fs,gs */
        int seg_idx = -1;
        for (int i = 0; seg_names[i]; i++) {
            if (streq(dst, seg_names[i])) { seg_idx = seg_codes[i]; break; }
        }
        if (seg_idx >= 0) {
            emit(0x8E); emit((uint8_t)(0xC0 | ((seg_idx & 7) << 3) | (rs & 7)));
            return 0;
        }
    }

    /* 8. mov r16, seg_reg (e.g. mov ax, ds) */
    if (is_seg_reg(src) && rd >= 0) {
        /* 8C /r: mov r/m16, seg_reg */
        static const char seg_codes[] = { 0, 1, 2, 3, 4, 5 };
        int seg_idx = -1;
        for (int i = 0; seg_names[i]; i++) {
            if (streq(src, seg_names[i])) { seg_idx = seg_codes[i]; break; }
        }
        if (seg_idx >= 0) {
            emit(0x8C); emit((uint8_t)(0xC0 | ((seg_idx & 7) << 3) | (rd & 7)));
            return 0;
        }
    }

    vga_print("asm: mov invalid operand\n");
    return -1;
}

/* generate: add */
static int gen_add(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    /* 1. add r/m32, r32 */
    if (rs >= 0) {
        if (rd >= 0) {
            emit(0x01); emit(modrm_rr(rs, rd));
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x01); emit_mem((uint8_t)rs, &mem);
            return 0;
        }
    }

    /* 2. add r32, r/m32 */
    if (rd >= 0 && parse_mem(src, &mem)) {
        emit(0x03); emit_mem((uint8_t)rd, &mem);
        return 0;
    }

    /* 3. add r/m32, imm */
    uint32_t imm;
    if (parse_int(src, &imm)) {
        if (rd >= 0) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit(modrm_rr(0, rd)); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit(modrm_rr(0, rd)); emit32(imm);
            }
            return 0;
        } else if (parse_mem(dst, &mem)) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit_mem(0, &mem); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit_mem(0, &mem); emit32(imm);
            }
            return 0;
        }
    }
    return -1;
}

/* generate: sub */
static int gen_sub(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    /* 1. sub r/m32, r32 */
    if (rs >= 0) {
        if (rd >= 0) {
            emit(0x29); emit(modrm_rr(rs, rd));
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x29); emit_mem((uint8_t)rs, &mem);
            return 0;
        }
    }

    /* 2. sub r32, r/m32 */
    if (rd >= 0 && parse_mem(src, &mem)) {
        emit(0x2B); emit_mem((uint8_t)rd, &mem);
        return 0;
    }

    /* 3. sub r/m32, imm */
    uint32_t imm;
    if (parse_int(src, &imm)) {
        if (rd >= 0) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit(modrm_rr(5, rd)); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit(modrm_rr(5, rd)); emit32(imm);
            }
            return 0;
        } else if (parse_mem(dst, &mem)) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit_mem(5, &mem); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit_mem(5, &mem); emit32(imm);
            }
            return 0;
        }
    }
    return -1;
}

/* generate: cmp */
static int gen_cmp(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    /* 1. cmp r/m32, r32 */
    if (rs >= 0) {
        if (rd >= 0) {
            emit(0x39); emit(modrm_rr(rs, rd));
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x39); emit_mem((uint8_t)rs, &mem);
            return 0;
        }
    }

    /* 2. cmp r32, r/m32 */
    if (rd >= 0 && parse_mem(src, &mem)) {
        emit(0x3B); emit_mem((uint8_t)rd, &mem);
        return 0;
    }

    /* 3. cmp r/m32, imm */
    uint32_t imm;
    if (parse_int(src, &imm)) {
        if (rd >= 0) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit(modrm_rr(7, rd)); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit(modrm_rr(7, rd)); emit32(imm);
            }
            return 0;
        } else if (parse_mem(dst, &mem)) {
            if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
                emit(0x83); emit_mem(7, &mem); emit((uint8_t)(int8_t)(int32_t)imm);
            } else {
                emit(0x81); emit_mem(7, &mem); emit32(imm);
            }
            return 0;
        }
    }
    return -1;
}

/* generate: xor */
static int gen_xor(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    if (rd >= 0 && rs >= 0) {
        emit(0x31); emit(modrm_rr(rs, rd));
        return 0;
    } else if (rs >= 0 && parse_mem(dst, &mem)) {
        emit(0x31); emit_mem((uint8_t)rs, &mem);
        return 0;
    } else if (rd >= 0 && parse_mem(src, &mem)) {
        emit(0x33); emit_mem((uint8_t)rd, &mem);
        return 0;
    }
    return -1;
}

/* generate: and */
static int gen_and(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    if (rs >= 0) {
        if (rd >= 0) {
            emit(0x21); emit(modrm_rr(rs, rd));
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x21); emit_mem((uint8_t)rs, &mem);
            return 0;
        }
    }

    uint32_t imm;
    if (parse_int(src, &imm)) {
        if (rd >= 0) {
            emit(0x81); emit(modrm_rr(4, rd)); emit32(imm);
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x81); emit_mem(4, &mem); emit32(imm);
            return 0;
        }
    }
    return -1;
}

/* generate: or */
static int gen_or(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    if (rs >= 0) {
        if (rd >= 0) {
            emit(0x09); emit(modrm_rr(rs, rd));
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x09); emit_mem((uint8_t)rs, &mem);
            return 0;
        }
    }

    uint32_t imm;
    if (parse_int(src, &imm)) {
        if (rd >= 0) {
            emit(0x81); emit(modrm_rr(1, rd)); emit32(imm);
            return 0;
        } else if (parse_mem(dst, &mem)) {
            emit(0x81); emit_mem(1, &mem); emit32(imm);
            return 0;
        }
    }
    return -1;
}

/* generate: setcc with suffix parsing */
static int gen_setcc_full(char *mnem, char *ops) {
    /* mnem like "sete", "setne", "setl", "setle", "setg", "setge" */
    uint8_t op = 0;
    if (streq(mnem, "sete")) op = 0x94;       /* sete */
    else if (streq(mnem, "setz")) op = 0x94;  /* alias */
    else if (streq(mnem, "setne")) op = 0x95; /* setne */
    else if (streq(mnem, "setnz")) op = 0x95;
    else if (streq(mnem, "setl")) op = 0x9C;  /* setl */
    else if (streq(mnem, "setnge")) op = 0x9C;
    else if (streq(mnem, "setle")) op = 0x9E; /* setle */
    else if (streq(mnem, "setng")) op = 0x9E;
    else if (streq(mnem, "setg")) op = 0x9F;  /* setg */
    else if (streq(mnem, "setnle")) op = 0x9F;
    else if (streq(mnem, "setge")) op = 0x9D; /* setge */
    else if (streq(mnem, "setnl")) op = 0x9D;
    else if (streq(mnem, "setb")) op = 0x92;  /* setb */
    else if (streq(mnem, "setbe")) op = 0x96; /* setbe */
    else if (streq(mnem, "seta")) op = 0x97;  /* seta */
    else if (streq(mnem, "setae")) op = 0x93; /* setae */
    else return -1;

    /* setcc is 1-operand: setg al, setl eax, etc. */
    trim(ops); str_lower(ops);

    /* 8-bit register aliases - map to 32-bit register number for ModRM */
    int r = -1;
    if (streq(ops, "al") || streq(ops, "ax") || streq(ops, "eax")) r = 0;
    else if (streq(ops, "cl") || streq(ops, "cx") || streq(ops, "ecx")) r = 1;
    else if (streq(ops, "dl") || streq(ops, "dx") || streq(ops, "edx")) r = 2;
    else if (streq(ops, "bl") || streq(ops, "bx") || streq(ops, "ebx")) r = 3;
    else r = parse_reg(ops);

    if (r < 0) return -1;
    emit(0x0F);
    emit(op);
    emit((uint8_t)(0xC0 | (r & 7)));
    return 0;
}

/* generate: cmovcc - conditional move */
/* Format: cmovcc dst, src */
static int gen_cmovcc(char *mnem, char *ops) {
    uint8_t op = 0;
    if (streq(mnem, "cmove")) op = 0x44;
    else if (streq(mnem, "cmovz")) op = 0x44;
    else if (streq(mnem, "cmovne")) op = 0x45;
    else if (streq(mnem, "cmovnz")) op = 0x45;
    else if (streq(mnem, "cmovl")) op = 0x4C;
    else if (streq(mnem, "cmovle")) op = 0x4E;
    else if (streq(mnem, "cmovg")) op = 0x4F;
    else if (streq(mnem, "cmovge")) op = 0x4D;
    else return -1;

    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    if (rd < 0 || rs < 0) return -1;

    emit(0x0F);
    emit(op);
    emit(modrm_rr(rd, rs));
    return 0;
}

/* generate: movzx */
static int gen_movzx(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst);
    if (rd < 0) return -1;
    if (streq(src, "al")) {
        /* movzx r32, al: 0F B6 /r */
        emit(0x0F); emit(0xB6); emit(modrm_rr(rd, 0));
        return 0;
    }
    if (streq(src, "ax")) {
        /* movzx r32, ax: 0F B7 /r */
        emit(0x0F); emit(0xB7); emit(modrm_rr(rd, 0));
        return 0;
    }
    return -1;
}

/* generate: cdq - sign-extend eax to edx:eax */
static int gen_cdq(char *ops) {
    (void)ops;
    emit(0x99);
    return 0;
}

/* generate: test r/m32, r32 (or test r32, r32) - logical AND, set flags */
static int gen_test(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    if (rd >= 0 && rs >= 0) {
        /* test r32, r32: 85 /r */
        emit(0x85); emit(modrm_rr(rd, rs));
        return 0;
    }
    /* test r32, imm32: F7 /0 id */
    if (rd >= 0) {
        uint32_t imm;
        if (parse_int(src, &imm)) {
            emit(0xF7); emit((uint8_t)(0xC0 | (rd & 7)));
            emit32(imm);
            return 0;
        }
    }
    return -1;
}

/* generate: push */
static int gen_push(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r >= 0) {
        /* push r32: 50+rd */
        emit((uint8_t)(0x50 + r));
        return 0;
    }
    uint32_t imm;
    if (!parse_int(ops, &imm)) return -1;
    if ((int32_t)imm >= -128 && (int32_t)imm <= 127) {
        /* push imm8: 6A ib */
        emit(0x6A); emit((uint8_t)(int8_t)(int32_t)imm);
    } else {
        /* push imm32: 68 id */
        emit(0x68); emit32(imm);
    }
    return 0;
}

/* generate: pop */
static int gen_pop(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r < 0) return -1;
    /* pop r32: 58+rd */
    emit((uint8_t)(0x58 + r));
    return 0;
}

/* generate: inc */
static int gen_inc(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r < 0) return -1;
    /* inc r32: 40+rd */
    emit((uint8_t)(0x40 + r));
    return 0;
}

/* generate: dec */
static int gen_dec(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r < 0) return -1;
    /* dec r32: 48+rd */
    emit((uint8_t)(0x48 + r));
    return 0;
}

/* generate: neg r32 - two's complement negation: F7 /3 */
static int gen_neg(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r < 0) return -1;
    emit(0xF7); emit((uint8_t)(0xD8 | (r & 7)));
    return 0;
}

/* generate: div r32 - unsigned divide edx:eax by r32: F7 /6 */
static int gen_div(char *ops) {
    trim(ops); str_lower(ops);
    int r = parse_reg(ops);
    if (r < 0) return -1;
    emit(0xF7); emit((uint8_t)(0xF0 | (r & 7)));
    return 0;
}

/* generate: imul */
static int gen_imul(char *ops) {
    char dst[32], src[32];
    if (!split_ops(ops, dst, src)) return -1;
    str_lower(dst); str_lower(src);
    int rd = parse_reg(dst), rs = parse_reg(src);
    mem_op_t mem;

    if (rd < 0) return -1;

    if (rs >= 0) {
        /* imul r32, r32: 0F AF /r */
        emit(0x0F); emit(0xAF); emit(modrm_rr(rd, rs));
        return 0;
    } else if (parse_mem(src, &mem)) {
        /* imul r32, r/m32: 0F AF /r */
        emit(0x0F); emit(0xAF); emit_mem((uint8_t)rd, &mem);
        return 0;
    } else {
        uint32_t imm;
        if (parse_int(src, &imm)) {
            /* imul r32, r/m32, imm32 (simplified as imul r32, imm32): 69 /r id */
            emit(0x69); emit(modrm_rr(rd, rd)); emit32(imm);
            return 0;
        }
    }
    return -1;
}

/* generate: jmp dan conditional jump */
static int gen_jmp(char *ops, uint8_t short_op) {
    trim(ops);
    int target = find_label(ops);

    if (target >= 0) {
        /* label sudah diketahui, hitung relatif */
        int rel8 = target - (code_len + 2);
        if (rel8 >= -128 && rel8 <= 127) {
            emit(short_op); emit((uint8_t)(int8_t)rel8);
        } else if (short_op == 0xEB) {
            /* jmp biasa bisa pakai near 32-bit */
            int rel32 = target - (code_len + 5);
            emit(0xE9); emit32((uint32_t)(int32_t)rel32);
        } else {
            vga_print("asm: conditional jump terlalu jauh\n");
            return -1;
        }
        return 0;
    }

    /* forward jump - belum tau target */
    if (short_op == 0xEB) {
        /* jmp: pakai near supaya bisa jangkau jauh */
        add_patch(code_len + 1, code_len + 5, ops, 1);
        emit(0xE9); emit32(0);
    } else {
        /* conditional: pakai short, batas 127 byte */
        add_patch(code_len + 1, code_len + 2, ops, 0);
        emit(short_op); emit(0x00);
    }
    return 0;
}

/* generate: call */
static int gen_call(char *ops) {
    trim(ops);
    int target = find_label(ops);
    if (target >= 0) {
        int rel32 = target - (code_len + 5);
        emit(0xE8); emit32((uint32_t)(int32_t)rel32);
        return 0;
    }
    add_patch(code_len + 1, code_len + 5, ops, 1);
    emit(0xE8); emit32(0);
    return 0;
}

/* proses satu baris assembly, return 0 ok, -1 error */
static int process_line(char *line) {
    trim(line);
    if (!line[0] || line[0] == ';') return 0;

    /* potong komentar inline */
    for (int i = 0; line[i]; i++) {
        if (line[i] == ';') { line[i] = 0; break; }
    }
    trim(line);
    if (!line[0]) return 0;

    /* cek label di awal baris (mis: "hang:" atau "hang: jmp hang") */
    int colon_pos = -1;
    for (int j = 0; line[j]; j++) {
        if (line[j] == ':') { colon_pos = j; break; }
        if (line[j] == ' ' || line[j] == '\t') break;
    }
    if (colon_pos > 0) {
        char label_name[32];
        strncopy(label_name, line, colon_pos + 1);
        label_name[colon_pos] = 0;
        trim(label_name);
        /* don't lowercase - keep case as codegen uses uppercase */
        if (num_labels < MAX_LABELS) {
            strncopy(labels[num_labels].name, label_name, 32);
            labels[num_labels].pos = code_len;
            num_labels++;
        }
        /* sisa baris setelah ':' dijadikan instruksi */
        char *rest = line + colon_pos + 1;
        trim(rest);
        if (!rest[0]) return 0;
        return process_line(rest);
    }

    /* pisah mnemonic dan operand */
    char mnem[16] = {0};
    int i = 0;
    while (line[i] && line[i] != ' ' && line[i] != '\t') i++;
    if (i >= 16) i = 15;
    strncopy(mnem, line, i + 1);
    mnem[i] = 0;
    char *ops = line + i;
    trim(ops);
    str_lower(mnem);

    /* instruksi tanpa operand */
    if (streq(mnem, "nop"))   { emit(0x90); return 0; }
    if (streq(mnem, "ret"))   { emit(0xC3); return 0; }
    if (streq(mnem, "hlt"))   { emit(0xF4); return 0; }
    if (streq(mnem, "pusha")) { emit(0x60); return 0; }
    if (streq(mnem, "popa"))  { emit(0x61); return 0; }
    if (streq(mnem, "sti"))   { emit(0xFB); return 0; }
    if (streq(mnem, "cli"))   { emit(0xFA); return 0; }

    /* int imm8: CD ib */
    if (streq(mnem, "int")) {
        trim(ops);
        uint32_t imm;
        if (!parse_int(ops, &imm)) return -1;
        emit(0xCD); emit((uint8_t)imm);
        return 0;
    }

    /* instruksi data (db) - mendukung mixed format:
     *   db 'Hello', 10, 0       (string + angka)
     *   db "Hello World", 0     (double-quote string + angka)
     *   db 0x41, 0x42, 0        (angka saja)
     *   db 'A'                  (single char)
     */
    if (streq(mnem, "db")) {
        char *p = ops;
        while (*p) {
            /* skip whitespace dan koma */
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            if (!*p) break;

            if (*p == '\'' || *p == '"') {
                /* string literal: emit semua byte sampai closing quote */
                char quote = *p;
                p++;
                while (*p && *p != quote) {
                    emit((uint8_t)*p);
                    p++;
                }
                if (*p == quote) p++; /* skip closing quote */
            } else {
                /* numeric value: parse sampai koma atau akhir string */
                char *end = p;
                while (*end && *end != ',') end++;
                /* trim trailing spaces */
                char *trimmed = end;
                while (trimmed > p && (trimmed[-1] == ' ' || trimmed[-1] == '\t')) trimmed--;
                char save = *trimmed;
                *trimmed = 0;
                uint32_t val;
                if (parse_int(p, &val)) {
                    emit((uint8_t)(val & 0xFF));
                }
                *trimmed = save;
                p = end;
            }
        }
        return 0;
    }

    /* instruksi dengan operand */
    if (streq(mnem, "mov"))   return gen_mov(ops);
    if (streq(mnem, "add"))   return gen_add(ops);
    if (streq(mnem, "sub"))   return gen_sub(ops);
    if (streq(mnem, "cmp"))   return gen_cmp(ops);
    if (streq(mnem, "xor"))   return gen_xor(ops);
    if (streq(mnem, "and"))   return gen_and(ops);
    if (streq(mnem, "or"))    return gen_or(ops);
    if (streq(mnem, "push"))  return gen_push(ops);
    if (streq(mnem, "pop"))   return gen_pop(ops);
    if (streq(mnem, "inc"))   return gen_inc(ops);
    if (streq(mnem, "dec"))   return gen_dec(ops);
    if (streq(mnem, "imul"))  return gen_imul(ops);
    if (streq(mnem, "idiv")) {
        trim(ops); str_lower(ops);
        int r = parse_reg(ops);
        if (r < 0) return -1;
        /* idiv r32: F7 /7 */
        emit(0xF7);
        emit((uint8_t)(0xF8 | (r & 7)));
        return 0;
    }
    if (streq(mnem, "call"))  return gen_call(ops);
    if (streq(mnem, "movzx")) return gen_movzx(ops);
    if (streq(mnem, "cdq"))   return gen_cdq(ops);
    if (streq(mnem, "test"))  return gen_test(ops);
    if (streq(mnem, "neg"))   return gen_neg(ops);
    if (streq(mnem, "div"))   return gen_div(ops);

    /* setcc (set al on condition) */
    if (mnem[0] == 's' && mnem[1] == 'e' && mnem[2] == 't' && (mnem[3] != 0)) {
        if (gen_setcc_full(mnem, ops) == 0) return 0;
    }

    /* cmovcc */
    if (mnem[0] == 'c' && mnem[1] == 'm' && mnem[2] == 'o' && mnem[3] == 'v' && mnem[4] != 0) {
        if (gen_cmovcc(mnem, ops) == 0) return 0;
    }

    /* jump */
    if (streq(mnem, "jmp"))  return gen_jmp(ops, 0xEB);
    if (streq(mnem, "je")  || streq(mnem, "jz"))   return gen_jmp(ops, 0x74);
    if (streq(mnem, "jne") || streq(mnem, "jnz"))  return gen_jmp(ops, 0x75);
    if (streq(mnem, "jg")  || streq(mnem, "jnle")) return gen_jmp(ops, 0x7F);
    if (streq(mnem, "jl")  || streq(mnem, "jnge")) return gen_jmp(ops, 0x7C);
    if (streq(mnem, "jge") || streq(mnem, "jnl"))  return gen_jmp(ops, 0x7D);
    if (streq(mnem, "jle") || streq(mnem, "jng"))  return gen_jmp(ops, 0x7E);

    vga_print("asm: instruksi tidak dikenal: ");
    vga_print(mnem);
    vga_print("\n");
    return -1;
}

/* terapkan semua patch yang belum terselesaikan */
static int apply_patches(void) {
    for (int i = 0; i < num_patches; i++) {
        int target = find_label(patches[i].target);

        if (target >= 0) {
            /* label lokal ditemukan */
            if (patches[i].type == 0) {
                /* rel8 */
                int rel = target - patches[i].from;
                if (rel < -128 || rel > 127) {
                    vga_print("asm: jump terlalu jauh, pake jmp\n");
                    return -1;
                }
                code_buf[patches[i].pos] = (uint8_t)(int8_t)rel;
            } else if (patches[i].type == 1) {
                /* rel32 */
                int32_t rel = (int32_t)(target - patches[i].from);
                code_buf[patches[i].pos + 0] = (uint8_t)(rel & 0xFF);
                code_buf[patches[i].pos + 1] = (uint8_t)((rel >> 8) & 0xFF);
                code_buf[patches[i].pos + 2] = (uint8_t)((rel >> 16) & 0xFF);
                code_buf[patches[i].pos + 3] = (uint8_t)((rel >> 24) & 0xFF);
            } else if (patches[i].type == 2) {
                /* abs32 (absolute address CODE_VIRT + target) */
                uint32_t abs_addr = CODE_VIRT + target;
                code_buf[patches[i].pos + 0] = (uint8_t)(abs_addr & 0xFF);
                code_buf[patches[i].pos + 1] = (uint8_t)((abs_addr >> 8) & 0xFF);
                code_buf[patches[i].pos + 2] = (uint8_t)((abs_addr >> 16) & 0xFF);
                code_buf[patches[i].pos + 3] = (uint8_t)((abs_addr >> 24) & 0xFF);
            }
        } else {
            /* cek extern symbol (fungsi kernel/klibc) */
            uint32_t ext_addr = find_extern(patches[i].target);
            if (ext_addr == 0) {
                vga_print("asm: label tidak ditemukan: ");
                vga_print(patches[i].target);
                vga_print("\n");
                return -1;
            }

            if (patches[i].type == 0) {
                /* rel8 - extern gak bisa pake short jump */
                vga_print("asm: extern terlalu jauh untuk rel8: ");
                vga_print(patches[i].target);
                vga_print("\n");
                return -1;
            } else if (patches[i].type == 1) {
                /* rel32 - hitung relative call/jmp ke alamat extern */
                uint32_t from_addr = CODE_VIRT + (uint32_t)patches[i].from;
                int32_t rel = (int32_t)(ext_addr - from_addr);
                code_buf[patches[i].pos + 0] = (uint8_t)(rel & 0xFF);
                code_buf[patches[i].pos + 1] = (uint8_t)((rel >> 8) & 0xFF);
                code_buf[patches[i].pos + 2] = (uint8_t)((rel >> 16) & 0xFF);
                code_buf[patches[i].pos + 3] = (uint8_t)((rel >> 24) & 0xFF);
            } else if (patches[i].type == 2) {
                /* abs32 - langsung pake alamat extern */
                code_buf[patches[i].pos + 0] = (uint8_t)(ext_addr & 0xFF);
                code_buf[patches[i].pos + 1] = (uint8_t)((ext_addr >> 8) & 0xFF);
                code_buf[patches[i].pos + 2] = (uint8_t)((ext_addr >> 16) & 0xFF);
                code_buf[patches[i].pos + 3] = (uint8_t)((ext_addr >> 24) & 0xFF);
            }
        }
    }
    return 0;
}

int asm_assemble(const char *code, void **exec_addr) {
    /* reset semua state assembler */
    code_len    = 0;
    num_labels  = 0;
    num_patches = 0;
    for (int i = 0; i < CODE_SIZE; i++) code_buf[i] = 0;

    /* buffer lokal yang bisa di-modif */
    static char input_buf[CODE_SIZE];
    uint32_t input_len = strlen(code);
    if (input_len >= CODE_SIZE) input_len = CODE_SIZE - 1;
    memcpy(input_buf, code, input_len);
    input_buf[input_len] = 0;

    /* proses baris per baris */
    char line[MAX_LINE_LEN];
    int pos = 0;
    while (pos <= (int)input_len) {
        int start = pos;
        while (pos < (int)input_len && input_buf[pos] != '\n') pos++;
        int end = pos;
        pos++;

        int llen = end - start;
        if (llen >= MAX_LINE_LEN) llen = MAX_LINE_LEN - 1;
        memcpy(line, input_buf + start, (uint32_t)llen);
        line[llen] = 0;

        if (process_line(line) < 0) {
            return -1;
        }
    }

    /* sisip ret di akhir kalo blm ada terminator (biar selalu balik ke shell) */
    if (code_len > 0) {
        uint8_t last = code_buf[code_len - 1];
        if (last != 0xC3 && last != 0xEB && last != 0xE9 && last != 0xF4) {
            emit(0xC3);
        }
    }

    /* patch semua forward reference */
    if (apply_patches() < 0) return -1;
    if (code_len == 0) return 0;

    /* alokasi halaman fisik buat kode executable */
    uint32_t phys = pmm_alloc_page();
    if (phys == 0) {
        vga_print("asm: gagal alokasi halaman memori\n");
        return -1;
    }

    /* map ke virtual address */
    page_map(CODE_VIRT, phys, PTE_PRESENT | PTE_WRITE | PTE_USER);

    /* flush TLB SEBELUM akses halaman yang baru di-map */
    asm volatile("invlpg (%0)" : : "r"(CODE_VIRT) : "memory");

    /* salin kode mesin ke virtual address */
    uint8_t *dest = (uint8_t *)CODE_VIRT;
    memcpy(dest, code_buf, (uint32_t)code_len);

    *exec_addr = dest;
    return code_len;
}

/* baca satu baris dari keyboard, return panjang */
static int read_line(char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c = keyboard_getchar();
        vga_putc(c);
        if (c == '\n') break;
        if (c == '\b') { if (i > 0) i--; continue; }
        buf[i++] = c;
    }
    buf[i] = 0;
    return i;
}

/* tampilkan hex dump dari byte array */
static void hex_dump(const uint8_t *data, int len) {
    char buf[4];
    for (int i = 0; i < len && i < 32; i++) {
        if (data[i] < 0x10) vga_putc('0');
        itoa(data[i], buf, 16);
        vga_print(buf);
        vga_putc(' ');
    }
    if (len > 32) vga_print("...");
}

int asm_run(void) {
    static char all_code[CODE_SIZE];
    int total = 0;
    char line[MAX_LINE_LEN];

    vga_print("=== Mode Assembler OaSis ===\n");
    vga_print("Dukung: mov, add, sub, cmp, xor, and, or, push, pop, inc, dec\n");
    vga_print("        jmp, je, jne, jg, jl, jge, jle, call, ret, int, nop, hlt\n");
    vga_print("Register: eax ecx edx ebx esp ebp esi edi\n");
    vga_print("Komentar: diawali ';'\n");
    vga_print("Akhiri dengan '---'\n\n");

    while (1) {
        vga_print("> ");
        read_line(line, MAX_LINE_LEN);

        /* cek terminasi */
        if (streq(line, "---")) break;

        /* tambah ke buffer gabungan */
        uint32_t llen = strlen(line);
        if (total + (int)llen + 1 >= CODE_SIZE - 1) {
            vga_print("asm: kode terlalu panjang\n");
            break;
        }
        memcpy(all_code + total, line, llen);
        total += (int)llen;
        all_code[total++] = '\n';
    }

    all_code[total] = 0;

    if (total == 0) {
        vga_print("asm: tidak ada kode yang diinput\n");
        return -1;
    }

    vga_print("\nMengassemble kode...\n");

    void *exec_addr = 0;
    int result = asm_assemble(all_code, &exec_addr);

    if (result < 0) {
        vga_print("asm: assembling gagal\n");
        return -1;
    }

    if (result == 0) {
        vga_print("asm: tidak ada kode yang di-generate\n");
        return -1;
    }

    /* info hasil */
    char buf[16];
    vga_print("Kode mesin: ");
    itoa(result, buf, 10);
    vga_print(buf);
    vga_print(" byte di alamat 0x");
    itoa((uint32_t)(uint32_t)exec_addr, buf, 16);
    vga_print(buf);
    vga_print("\nBytes: ");
    hex_dump((uint8_t *)exec_addr, result);
    vga_print("\n");

    vga_print("Menjalankan...\n");

    /* cast ke fungsi dan panggil */
    void (*fn)(void) = (void (*)(void))exec_addr;
    asm volatile("sti");
    fn();

    vga_print("\n[selesai]\n");
    return 0;
}

int asm_run_file(const char *path) {
    dbg_print("\n[nasm] mulai untuk file=");
    dbg_print(path);
    dbg_print("\n");

    /* reset warna VGA dulu biar gak warisan warna dari editor/prompt */
    vga_set_color(15, 0);

    int fd = vfs_open(path, VFS_O_READ);
    dbg_print("[nasm] vfs_open fd=");
    dbg_dec(fd);
    dbg_print("\n");
    if (fd < 0) {
        vga_print("nasm: gagal buka file '");
        vga_print(path);
        vga_print("'\n");
        return -1;
    }

    /* clear buffer dulu (static, jadi bisa ada sisa run sebelumnya) */
    static char file_buf[CODE_SIZE];
    for (int i = 0; i < CODE_SIZE; i++) file_buf[i] = 0;

    int n = vfs_read(fd, file_buf, CODE_SIZE - 1);
    dbg_print("[nasm] vfs_read n=");
    dbg_dec(n);
    dbg_print("\n");
    vfs_close(fd);

    if (n <= 0) {
        vga_print("nasm: file kosong atau error baca (n=");
        char dbuf[16];
        itoa(n, dbuf, 10);
        vga_print(dbuf);
        vga_print(")\n");
        return -1;
    }
    file_buf[n] = 0;

    dbg_print("[nasm] isi file:\n>>>\n");
    dbg_print(file_buf);
    dbg_print("\n<<<\n");

    /* tampilkan isi file biar user bisa verify */
    vga_print("=== Isi file ");
    vga_print(path);
    vga_print(" (");
    char buf[16];
    itoa(n, buf, 10);
    vga_print(buf);
    vga_print(" byte) ===\n");
    vga_print(file_buf);
    vga_print("\n=== Akhir isi file ===\n\n");

    vga_print("Mengassemble ");
    vga_print(path);
    vga_print("...\n");

    void *exec_addr = 0;
    int result = asm_assemble(file_buf, &exec_addr);
    dbg_print("[nasm] asm_assemble result=");
    dbg_dec(result);
    dbg_print(" exec_addr=0x");
    dbg_hex((uint32_t)exec_addr);
    dbg_print("\n");

    if (result < 0) {
        vga_print("nasm: assembling gagal\n");
        return -1;
    }
    if (result == 0) {
        vga_print("nasm: tidak ada kode yang di-generate\n");
        return -1;
    }

    /* dump kode mesin */
    dbg_print("[nasm] kode mesin: ");
    for (int i = 0; i < result; i++) {
        uint8_t b = ((uint8_t *)exec_addr)[i];
        if (b < 0x10) dbg_putc('0');
        dbg_hex((uint32_t)b);
        dbg_putc(' ');
    }
    dbg_print("\n");

    /* info hasil */
    vga_print("Kode mesin: ");
    itoa(result, buf, 10);
    vga_print(buf);
    vga_print(" byte di alamat 0x");
    itoa((uint32_t)(uint32_t)exec_addr, buf, 16);
    vga_print(buf);
    vga_print("\nBytes: ");
    hex_dump((uint8_t *)exec_addr, result);
    vga_print("\n");

    vga_print("Menjalankan...\n");
    dbg_print("[nasm] mau call exec_addr=0x");
    dbg_hex((uint32_t)exec_addr);
    dbg_print("\n");
    /* reset warna lagi sebelum eksekusi, kalau-kalau code user gak set warna */
    vga_set_color(15, 0);

    void (*fn)(void) = (void (*)(void))exec_addr;
    asm volatile("sti"); /* pastikan interrupt nyala */
    fn();

    dbg_print("[nasm] balik dari fn()\n");
    /* reset warna setelah eksekusi user code */
    vga_set_color(15, 0);
    vga_print("\n[selesai]\n");
    return 0;
}

---
layout: default
title: changelog
---

# Changelog

## 2026-06-29 — Assembler parse_int whitespace fix

**Bug**: The assembler's `parse_int()` function did not handle leading whitespace. The occ codegen emits parameter loads like `[ebp + 8]` (with a space after `+`). `parse_int(" 8")` failed on the space character, causing the offset to default to 0. This meant `[ebp + 8]` was assembled as `[ebp + 0]` — reading the saved frame pointer instead of the actual function argument.

Every function parameter access `[ebp + 8]`, `[ebp + 12]`, etc. silently read the wrong stack slot, so `add(3, 4)` returned a garbage address (the old EBP) instead of 7.

**Fix**: Added whitespace skipping at the start of `parse_int()`. Made hex/decimal parsing loops tolerate trailing whitespace.

**Files**: `src/kernel/apps/asm.c`

## 2026-06-29 — Ring 3 page fault fixes

**Bug**: Running occ-generated code via the `user` command caused page fault (INT 14, error code 0x5, CR2=0x10d4a9). User-mode code could not call into kernel-only pages because the user page directory stripped `PTE_USER` from PDE 0 (identity-mapped kernel pages at 0-4MB).

**Fixes**:
- Mark PDE 0 entries as user-accessible (`|= PTE_USER`) in user page directory
- Added symbol remapping in assembler for user mode (`_printf` → `_usr_printf`, etc.)
- `run_user_test` now calls `asm_assemble_user()` instead of `asm_assemble()`

**Files**: `src/kernel/core/paging.c`, `src/kernel/apps/asm.c`, `src/kernel/tasks/task_user.c`, `include/asm.h`

## 2026-06-29 — Codegen entry trampoline

**Bug**: Generated assembly started execution at the first function in the file (e.g. `_add`) instead of `_main`. Without an explicit entry point, the program returned immediately without ever reaching `main()`.

**Fix**: Added `call _main; ret` trampoline at the start of codegen output.

**File**: `src/kernel/lib/codegen.c`

## 2026-06-29 — AST_ASSIGNMENT fall-through

**Bug**: Missing `break` in the `gen_statement` switch caused assignment expressions to fall through into the `AST_IF` case, corrupting codegen for assignment statements.

**Fix**: Added `gen_assignment(cg, stmt); break;` in the appropriate case.

**File**: `src/kernel/lib/codegen.c`

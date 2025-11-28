#pragma once

#include <stdint.h>

/* Forward declaration */
struct trap_frame;

/* ELF machine types */
#define EM_RISCV 0xF3  /* RISC-V architecture */

/* ELF file types */
#define ET_EXEC 2  /* Executable file */

int elf_load_exec(const char *path, const char *argv[], int argc);
int elf_exec_replace(const char *path, const char *argv[], int argc, struct trap_frame *tf);

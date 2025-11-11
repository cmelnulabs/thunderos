#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/kstring.h"
#include "fs/vfs.h"
#include "kernel/process.h"
#include "mm/kmalloc.h"
#include "hal/hal_uart.h"

#define ELF_MAGIC 0x464C457F
#define PT_LOAD   1

typedef struct {
    uint32_t magic;
    uint8_t  class;
    uint8_t  data;
    uint8_t  version;
    uint8_t  os_abi;
    uint8_t  abi_version;
    uint8_t  pad[7];
    uint16_t type;
    uint16_t machine;
    uint32_t version2;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} elf64_ehdr_t;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} elf64_phdr_t;

/**
 * Load ELF binary from filesystem and create process
 * 
 * @param path Path to ELF binary
 * @param argv Argument array
 * @param argc Argument count
 * @return PID of new process, or negative error code
 */
int elf_load_exec(const char *path, const char *argv[], int argc) {
    (void)argv;
    (void)argc;
    
    hal_uart_puts("elf_loader: Loading ");
    hal_uart_puts(path);
    hal_uart_puts("\n");
    
    // Open file
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        hal_uart_puts("elf_loader: Failed to open file\n");
        return -1;
    }
    
    // Read ELF header
    elf64_ehdr_t ehdr;
    if (vfs_read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        hal_uart_puts("elf_loader: Failed to read ELF header\n");
        vfs_close(fd);
        return -2;
    }
    
    // Verify ELF magic
    if (ehdr.magic != ELF_MAGIC) {
        hal_uart_puts("elf_loader: Invalid ELF magic\n");
        vfs_close(fd);
        return -3;
    }
    
    hal_uart_puts("elf_loader: Valid ELF file detected\n");
    hal_uart_puts("elf_loader: Entry point: 0x");
    hal_uart_put_hex(ehdr.entry);
    hal_uart_puts("\n");
    
    // For now, just read the file and create a simple process
    // Full implementation would parse program headers and load segments
    
    vfs_close(fd);
    
    hal_uart_puts("elf_loader: ELF loading not fully implemented yet\n");
    return -10;
}

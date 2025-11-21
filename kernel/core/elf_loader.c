#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/kstring.h"
#include "kernel/errno.h"
#include "kernel/elf_loader.h"
#include "fs/vfs.h"
#include "kernel/process.h"
#include "mm/kmalloc.h"
#include "mm/pmm.h"
#include "mm/paging.h"
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
 * @param argv Argument array (unused)
 * @param argc Argument count (unused)
 * @return PID of new process, or -1 on error (errno set)
 */
int elf_load_exec(const char *path, const char *argv[], int argc) {
    (void)argv;
    (void)argc;
    
    /* Open file */
    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        /* errno already set by vfs_open */
        return -1;
    }
    
    /* Read ELF header */
    elf64_ehdr_t ehdr;
    if (vfs_read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr)) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Verify ELF magic */
    if (ehdr.magic != ELF_MAGIC) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EELF_MAGIC);
    }
    
    /* Verify it's a RISC-V executable */
    if (ehdr.machine != EM_RISCV) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EELF_ARCH);
    }
    
    /* Verify it's an executable */
    if (ehdr.type != ET_EXEC) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EELF_TYPE);
    }
    
    /* Read program headers */
    if (ehdr.phnum == 0 || ehdr.phnum > 16) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EELF_NOPHDR);
    }
    
    /* Allocate space for program headers */
    size_t phdrs_size = ehdr.phnum * sizeof(elf64_phdr_t);
    elf64_phdr_t *phdrs = kmalloc(phdrs_size);
    if (!phdrs) {
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Seek to program headers */
    if (vfs_seek(fd, ehdr.phoff, SEEK_SET) < 0) {
        kfree(phdrs);
        vfs_close(fd);
        /* errno already set by vfs_seek */
        return -1;
    }
    
    /* Read all program headers */
    if (vfs_read(fd, phdrs, phdrs_size) != (int)phdrs_size) {
        kfree(phdrs);
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    // Find the total memory needed and lowest/highest addresses
    uint64_t min_addr = (uint64_t)-1;
    uint64_t max_addr = 0;
    
    for (int i = 0; i < ehdr.phnum; i++) {
        if (phdrs[i].type == PT_LOAD) {
            if (phdrs[i].vaddr < min_addr) {
                min_addr = phdrs[i].vaddr;
            }
            uint64_t seg_end = phdrs[i].vaddr + phdrs[i].memsz;
            if (seg_end > max_addr) {
                max_addr = seg_end;
            }
        }
    }
    
    if (min_addr == (uint64_t)-1) {
        kfree(phdrs);
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EELF_NOPHDR);
    }
    
    /* Allocate memory for the entire program (page-aligned) */
    size_t total_size = max_addr - min_addr;
    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t program_phys = pmm_alloc_pages(num_pages);
    if (!program_phys) {
        kfree(phdrs);
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Convert to virtual address for kernel access when loading segments */
    void *program_mem_virt = (void *)translate_phys_to_virt(program_phys);
    void *program_mem_phys = (void *)program_phys;
    
    /* Zero out the memory */
    kmemset(program_mem_virt, 0, total_size);
    
    /* Load each PT_LOAD segment */
    for (int i = 0; i < ehdr.phnum; i++) {
        if (phdrs[i].type != PT_LOAD) {
            continue;
        }
        
        /* Calculate offset into allocated memory (use virtual address for access) */
        void *dest = (uint8_t*)program_mem_virt + (phdrs[i].vaddr - min_addr);
        
        /* Seek to segment data in file */
        if (vfs_seek(fd, phdrs[i].offset, SEEK_SET) < 0) {
            pmm_free_pages(program_phys, num_pages);
            kfree(phdrs);
            vfs_close(fd);
            /* errno already set by vfs_seek */
            return -1;
        }
        
        /* Read segment data */
        if (phdrs[i].filesz > 0) {
            int nread = vfs_read(fd, dest, phdrs[i].filesz);
            if (nread != (int)phdrs[i].filesz) {
                pmm_free_pages(program_phys, num_pages);
                kfree(phdrs);
                vfs_close(fd);
                RETURN_ERRNO(THUNDEROS_EIO);
            }
        }
        
        // Zero out BSS (memsz > filesz)
        if (phdrs[i].memsz > phdrs[i].filesz) {
            size_t bss_size = phdrs[i].memsz - phdrs[i].filesz;
            kmemset((uint8_t*)dest + phdrs[i].filesz, 0, bss_size);
        }
    }
    
    /* Done with file and program headers */
    vfs_close(fd);
    kfree(phdrs);
    
    /* Extract program name from path for process name */
    const char *program_name = path;
    for (const char *p = path; *p; p++) {
        if (*p == '/') {
            program_name = p + 1;
        }
    }
    
    /* Create user process with loaded code and custom entry point */
    struct process *proc = process_create_elf(program_name, min_addr, program_mem_phys, total_size, ehdr.entry);
    
    if (!proc) {
        pmm_free_pages(program_phys, num_pages);
        RETURN_ERRNO(THUNDEROS_EPROC_INIT);
    }
    
    clear_errno();
    /* Return process ID */
    return proc->pid;
}

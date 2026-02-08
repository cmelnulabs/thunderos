#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "kernel/kstring.h"
#include "kernel/errno.h"
#include "kernel/constants.h"
#include "kernel/elf_loader.h"
#include "trap.h"
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

/**
 * elf_exec_replace - Replace current process with new ELF executable
 * 
 * This is the real exec() implementation - replaces the current process's
 * memory and starts execution at the new entry point. Does NOT return on success.
 * 
 * @param path Path to ELF executable
 * @param argv Argument array (NULL-terminated)
 * @param argc Argument count
 * @param tf Trap frame pointer (from syscall handler stack) to update with new entry point
 * @return -1 on error (and errno is set), does not return on success
 */
int elf_exec_replace(const char *path, const char *argv[], int argc, struct trap_frame *tf) {
    struct process *proc = process_current();
    if (!proc || !tf) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* CRITICAL: Copy path and argv to kernel buffers BEFORE we free user memory!
     * The path and argv pointers are in user space (e.g., shell's memory).
     * We will free the old process's pages during exec, which would make
     * them inaccessible. Copy now while they're still valid. */
    
    /* Copy path */
    char path_buf[ELF_PATH_BUFFER_SIZE];
    size_t path_len = 0;
    while (path[path_len] && path_len < sizeof(path_buf) - 1) {
        path_buf[path_len] = path[path_len];
        path_len++;
    }
    path_buf[path_len] = '\0';
    
    /* Copy argv to kernel buffers */
    #define MAX_EXEC_ARGS 16
    #define MAX_ARG_LEN 128
    char kargv_buf[MAX_EXEC_ARGS][MAX_ARG_LEN];
    const char *kargv[MAX_EXEC_ARGS + 1];
    int kargc = 0;
    
    if (argv) {
        for (int i = 0; i < argc && i < MAX_EXEC_ARGS && argv[i]; i++) {
            /* Copy each argument string */
            size_t arg_len = 0;
            while (argv[i][arg_len] && arg_len < MAX_ARG_LEN - 1) {
                kargv_buf[i][arg_len] = argv[i][arg_len];
                arg_len++;
            }
            kargv_buf[i][arg_len] = '\0';
            kargv[i] = kargv_buf[i];
            kargc++;
        }
    }
    kargv[kargc] = (char *)0;  /* NULL terminate */
    
    /* Use kernel buffer from now on */
    const char *kpath = path_buf;

    /* Open file */
    int fd = vfs_open(kpath, O_RDONLY);
    if (fd < 0) {
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
        return -1;
    }
    
    /* Read all program headers */
    if (vfs_read(fd, phdrs, phdrs_size) != (int)phdrs_size) {
        kfree(phdrs);
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Find memory range needed */
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
    
    /* Allocate memory for the new program */
    size_t total_size = max_addr - min_addr;
    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uintptr_t program_phys = pmm_alloc_pages(num_pages);
    if (!program_phys) {
        kfree(phdrs);
        vfs_close(fd);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Zero out the memory */
    void *program_mem = (void *)program_phys;
    kmemset(program_mem, 0, total_size);
    
    /* Load each PT_LOAD segment */
    for (int i = 0; i < ehdr.phnum; i++) {
        if (phdrs[i].type != PT_LOAD) {
            continue;
        }
        
        void *dest = (uint8_t*)program_mem + (phdrs[i].vaddr - min_addr);
        
        if (vfs_seek(fd, phdrs[i].offset, SEEK_SET) < 0) {
            pmm_free_pages(program_phys, num_pages);
            kfree(phdrs);
            vfs_close(fd);
            return -1;
        }
        
        if (phdrs[i].filesz > 0) {
            int nread = vfs_read(fd, dest, phdrs[i].filesz);
            if (nread != (int)phdrs[i].filesz) {
                pmm_free_pages(program_phys, num_pages);
                kfree(phdrs);
                vfs_close(fd);
                RETURN_ERRNO(THUNDEROS_EIO);
            }
        }
        
        /* Zero out BSS */
        if (phdrs[i].memsz > phdrs[i].filesz) {
            size_t bss_size = phdrs[i].memsz - phdrs[i].filesz;
            kmemset((uint8_t*)dest + phdrs[i].filesz, 0, bss_size);
        }
    }
    
    vfs_close(fd);
    kfree(phdrs);
    
    /* Now replace the current process's memory */
    
    /* 1. Free old CODE VMAs and their pages (but keep the stack!) */
    vm_area_t *vma = proc->vm_areas;
    vm_area_t *prev = NULL;
    
    while (vma) {
        vm_area_t *next = vma->next;
        
        /* Check if this is the user stack (typically at high addresses) */
        int is_stack = (vma->start >= ELF_USER_STACK_BASE && vma->start < ELF_USER_STACK_TOP);
        
        if (!is_stack) {
            /* Free physical pages for code/data VMAs */
            for (uint64_t addr = vma->start; addr < vma->end; addr += PAGE_SIZE) {
                uintptr_t paddr;
                if (virt_to_phys(proc->page_table, addr, &paddr) == 0) {
                    unmap_page(proc->page_table, addr);
                    pmm_free_page(paddr);
                }
            }
            
            /* Remove this VMA from the list */
            if (prev) {
                prev->next = next;
            } else {
                proc->vm_areas = next;
            }
            kfree(vma);
        } else {
            /* Keep stack VMA, just move prev pointer */
            prev = vma;
        }
        
        vma = next;
    }
    
    /* 2. Map new program into process's page table */
    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t vaddr = min_addr + (i * PAGE_SIZE);
        uintptr_t paddr = program_phys + (i * PAGE_SIZE);
        
        uint64_t flags = PTE_V | PTE_R | PTE_W | PTE_X | PTE_U;
        
        if (map_page(proc->page_table, vaddr, paddr, flags) != 0) {
            /* Critical error - can't recover */
            hal_uart_puts("FATAL: exec failed to map page at vaddr=0x");
            hal_uart_put_hex(vaddr);
            hal_uart_puts("\n");
            process_exit(-1);
        }
    }
    
    /* 3. Add VMA for the new program */
    if (process_add_vma(proc, min_addr, max_addr, VM_READ | VM_WRITE | VM_EXEC | VM_USER) != 0) {
        hal_uart_puts("FATAL: exec failed to add VMA\n");
        process_exit(-1);
    }
    
    /* 4. Setup new user stack and copy argv to it */
    /* Stack layout (growing down from USER_STACK_TOP):
     *   [argv strings]     - actual string data
     *   [padding for alignment]
     *   [argv[n] = NULL]   - NULL terminator
     *   [argv[n-1]]        - pointer to last arg string
     *   ...
     *   [argv[0]]          - pointer to first arg string (program name)
     *   SP points here ->
     */
    uint64_t sp = USER_STACK_TOP;
    uint64_t user_argv[MAX_EXEC_ARGS + 1];
    
    /* First pass: copy strings to stack (from top, growing down) */
    for (int i = kargc - 1; i >= 0; i--) {
        size_t len = 0;
        while (kargv[i][len]) len++;
        len++;  /* Include null terminator */
        
        sp -= len;
        sp &= ~7UL;  /* Align to 8 bytes */
        
        /* Copy string to user stack */
        char *dst = (char *)sp;
        for (size_t j = 0; j < len; j++) {
            dst[j] = kargv[i][j];
        }
        user_argv[i] = sp;  /* Remember where this string is */
    }
    user_argv[kargc] = 0;  /* NULL terminator */
    
    /* Second pass: copy argv pointers to stack */
    sp -= (kargc + 1) * sizeof(uint64_t);  /* Space for pointers + NULL */
    sp &= ~15UL;  /* Align to 16 bytes for RISC-V ABI */
    
    uint64_t argv_base = sp;
    uint64_t *argv_ptr = (uint64_t *)sp;
    for (int i = 0; i <= kargc; i++) {
        argv_ptr[i] = user_argv[i];
    }
    
    /* 5. Update trap frame to start at new entry point */
    /* CRITICAL: We modify the trap frame passed from syscall handler (on stack),
     * NOT proc->trap_frame. The trap handler will return using this tf. */
    tf->sepc = ehdr.entry;
    tf->sp = sp;  /* Stack pointer below argv */
    
    /* 6. Clear registers except sp */
    tf->ra = 0;
    tf->gp = 0;
    tf->tp = 0;
    tf->t0 = 0;
    tf->t1 = 0;
    tf->t2 = 0;
    tf->s0 = sp;  /* CRITICAL: s0 must be valid for -O0 compiled code */
    tf->s1 = 0;
    tf->a0 = kargc;  /* argc */
    tf->a1 = argv_base;  /* argv - pointer to argv array on user stack */
    tf->a2 = 0;
    tf->a3 = 0;
    tf->a4 = 0;
    tf->a5 = 0;
    tf->a6 = 0;
    tf->a7 = 0;
    tf->s2 = 0;
    tf->s3 = 0;
    tf->s4 = 0;
    tf->s5 = 0;
    tf->s6 = 0;
    tf->s7 = 0;
    tf->s8 = 0;
    tf->s9 = 0;
    tf->s10 = 0;
    tf->s11 = 0;
    tf->t3 = 0;
    tf->t4 = 0;
    tf->t5 = 0;
    tf->t6 = 0;
    
    /* Extract program name from path (use kernel copy, not user pointer!) */
    const char *program_name = kpath;
    for (const char *p = kpath; *p; p++) {
        if (*p == '/') {
            program_name = p + 1;
        }
    }
    kstrcpy(proc->name, program_name);
    
    /* CRITICAL: Switch to the new page table BEFORE returning to user mode!
     * We just replaced the memory mappings, but the CPU is still using the old
     * page table. Without this switch, we'd execute the old code. */
    switch_page_table(proc->page_table);
    
    clear_errno();
    
    /* SUCCESS: The trap return will jump to the new program's entry point
     * We return 0 to indicate success to sys_execve, but this value
     * should NOT be placed in a0 - the trap frame already has the correct values */
    return 0;
}

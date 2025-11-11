ELF Loader
==========

Overview
--------

The ELF (Executable and Linkable Format) loader (``kernel/core/elf_loader.c``) enables ThunderOS to load and execute programs stored on disk. It parses ELF64 files, maps program segments into memory, and creates new processes to run the executables.

ELF is the standard executable format for Linux and most Unix systems on RISC-V, x86-64, ARM, and other architectures.

ELF File Format
---------------

Structure Overview
~~~~~~~~~~~~~~~~~~

An ELF file consists of:

1. **ELF Header**: Metadata about the executable
2. **Program Headers**: Describe memory segments to load
3. **Section Headers**: Describe file sections (for linking/debugging)
4. **Segments/Sections**: Actual program code and data

.. code-block:: text

    ┌────────────────────────────────────────┐
    │  ELF Header (64 bytes)                 │
    │  - Magic number (0x7F 'E' 'L' 'F')     │
    │  - Architecture (RISC-V 64-bit)        │
    │  - Entry point address                 │
    │  - Program header table offset         │
    ├────────────────────────────────────────┤
    │  Program Header Table                  │
    │  - PT_LOAD: Loadable segment           │
    │  - PT_INTERP: Interpreter path         │
    │  - PT_DYNAMIC: Dynamic linking info    │
    ├────────────────────────────────────────┤
    │  .text Section (executable code)       │
    ├────────────────────────────────────────┤
    │  .rodata Section (read-only data)      │
    ├────────────────────────────────────────┤
    │  .data Section (initialized data)      │
    ├────────────────────────────────────────┤
    │  .bss Section (uninitialized data)     │
    ├────────────────────────────────────────┤
    │  Section Header Table (optional)       │
    └────────────────────────────────────────┘

ELF Header
~~~~~~~~~~

.. code-block:: c

    #define ELF_MAGIC   0x464C457F  // 0x7F 'E' 'L' 'F'
    
    struct elf64_header {
        uint32_t e_ident_magic;     // Magic number
        uint8_t  e_ident_class;     // 32-bit or 64-bit (2 = 64-bit)
        uint8_t  e_ident_data;      // Endianness (1 = little, 2 = big)
        uint8_t  e_ident_version;   // ELF version (1)
        uint8_t  e_ident_osabi;     // OS/ABI (0 = System V)
        uint8_t  e_ident_pad[8];    // Padding
        uint16_t e_type;            // Object file type
        uint16_t e_machine;         // Architecture
        uint32_t e_version;         // Version
        uint64_t e_entry;           // Entry point address
        uint64_t e_phoff;           // Program header table offset
        uint64_t e_shoff;           // Section header table offset
        uint32_t e_flags;           // Processor-specific flags
        uint16_t e_ehsize;          // ELF header size
        uint16_t e_phentsize;       // Program header entry size
        uint16_t e_phnum;           // Number of program headers
        uint16_t e_shentsize;       // Section header entry size
        uint16_t e_shnum;           // Number of section headers
        uint16_t e_shstrndx;        // Section header string table index
    };

**Key Fields:**

- ``e_ident_magic``: Must be ``0x464C457F`` (bytes: 0x7F, 'E', 'L', 'F')
- ``e_machine``: ``0xF3`` for RISC-V
- ``e_entry``: Virtual address to start execution (e.g., ``0x10000``)
- ``e_phoff``: Byte offset of program header table in file
- ``e_phnum``: Number of program headers (typically 2-4)

**ELF Types:**

.. code-block:: c

    #define ET_NONE   0  // No file type
    #define ET_REL    1  // Relocatable file (.o)
    #define ET_EXEC   2  // Executable file
    #define ET_DYN    3  // Shared object file (.so)
    #define ET_CORE   4  // Core dump file

ThunderOS loads ``ET_EXEC`` (statically linked executables).

Program Header
~~~~~~~~~~~~~~

Describes a segment to load into memory:

.. code-block:: c

    struct elf64_program_header {
        uint32_t p_type;      // Segment type
        uint32_t p_flags;     // Segment flags (read/write/execute)
        uint64_t p_offset;    // File offset
        uint64_t p_vaddr;     // Virtual address to load at
        uint64_t p_paddr;     // Physical address (ignored)
        uint64_t p_filesz;    // Size in file (bytes to read)
        uint64_t p_memsz;     // Size in memory (may be larger for .bss)
        uint64_t p_align;     // Alignment (must be power of 2)
    };

**Segment Types:**

.. code-block:: c

    #define PT_NULL    0  // Unused entry
    #define PT_LOAD    1  // Loadable segment
    #define PT_DYNAMIC 2  // Dynamic linking information
    #define PT_INTERP  3  // Interpreter path
    #define PT_NOTE    4  // Auxiliary information
    #define PT_PHDR    6  // Program header table

**Flags:**

.. code-block:: c

    #define PF_X  0x1  // Execute permission
    #define PF_W  0x2  // Write permission
    #define PF_R  0x4  // Read permission

**Typical Segments:**

1. **Code segment**: ``p_flags = PF_R | PF_X`` (read + execute, no write)
   - Contains ``.text`` section
   - ``p_vaddr`` = ``0x10000`` (typical starting address)
   
2. **Data segment**: ``p_flags = PF_R | PF_W`` (read + write, no execute)
   - Contains ``.data`` and ``.bss`` sections
   - ``p_memsz > p_filesz`` for ``.bss`` (zero-initialized data)

Loading Process
---------------

High-Level Algorithm
~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    int load_elf(const char *path) {
        // 1. Open ELF file
        int fd = vfs_open(path, O_RDONLY);
        
        // 2. Read and validate ELF header
        struct elf64_header header;
        vfs_read(fd, &header, sizeof(header));
        validate_elf_header(&header);
        
        // 3. Create new process
        struct process *proc = process_create(path);
        
        // 4. Create new page table for process
        uint64_t *page_table = create_page_table();
        proc->page_table = page_table;
        
        // 5. Load program segments
        for (int i = 0; i < header.e_phnum; i++) {
            struct elf64_program_header phdr;
            read_program_header(fd, &header, i, &phdr);
            
            if (phdr.p_type == PT_LOAD) {
                load_segment(fd, &phdr, page_table);
            }
        }
        
        // 6. Allocate user stack
        allocate_user_stack(proc, page_table);
        
        // 7. Set entry point
        proc->context.pc = header.e_entry;
        
        // 8. Set initial stack pointer
        proc->context.sp = USER_STACK_TOP;
        
        // 9. Mark process as ready
        proc->state = PROCESS_READY;
        
        vfs_close(fd);
        return 0;
    }

Implementation Details
----------------------

Validating ELF Header
~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    int validate_elf_header(struct elf64_header *header) {
        // 1. Check magic number
        if (header->e_ident_magic != ELF_MAGIC) {
            return -1;  // Not an ELF file
        }
        
        // 2. Check 64-bit
        if (header->e_ident_class != 2) {
            return -1;  // Not ELF64
        }
        
        // 3. Check little-endian
        if (header->e_ident_data != 1) {
            return -1;  // Wrong endianness
        }
        
        // 4. Check executable type
        if (header->e_type != ET_EXEC) {
            return -1;  // Not an executable
        }
        
        // 5. Check RISC-V architecture
        if (header->e_machine != 0xF3) {
            return -1;  // Wrong architecture
        }
        
        // 6. Validate entry point
        if (header->e_entry == 0) {
            return -1;  // No entry point
        }
        
        return 0;
    }

Loading a Segment
~~~~~~~~~~~~~~~~~

.. code-block:: c

    int load_segment(int fd, struct elf64_program_header *phdr,
                     uint64_t *page_table) {
        // 1. Calculate number of pages needed
        uint64_t start_addr = phdr->p_vaddr;
        uint64_t end_addr = phdr->p_vaddr + phdr->p_memsz;
        uint64_t start_page = start_addr & ~0xFFF;  // Round down to page
        uint64_t end_page = (end_addr + 0xFFF) & ~0xFFF;  // Round up
        uint32_t num_pages = (end_page - start_page) / PAGE_SIZE;
        
        // 2. Allocate physical pages
        for (uint32_t i = 0; i < num_pages; i++) {
            uint64_t virt_addr = start_page + (i * PAGE_SIZE);
            uint64_t phys_addr = pmm_alloc_page();
            
            if (phys_addr == 0) {
                return -1;  // Out of memory
            }
            
            // 3. Map page in page table
            uint64_t flags = PTE_VALID | PTE_USER;
            if (phdr->p_flags & PF_W) {
                flags |= PTE_WRITE;
            }
            if (phdr->p_flags & PF_X) {
                flags |= PTE_EXECUTE;
            }
            if (phdr->p_flags & PF_R) {
                flags |= PTE_READ;
            }
            
            map_page(page_table, virt_addr, phys_addr, flags);
            
            // 4. Zero the page
            memset((void*)phys_to_virt(phys_addr), 0, PAGE_SIZE);
        }
        
        // 5. Read segment data from file
        if (phdr->p_filesz > 0) {
            // Seek to file offset
            vfs_seek(fd, phdr->p_offset, SEEK_SET);
            
            // Read into mapped memory
            // Note: We read directly into the physical pages via kernel mapping
            char *buffer = kmalloc(phdr->p_filesz);
            vfs_read(fd, buffer, phdr->p_filesz);
            
            // Copy to mapped pages
            copy_to_user_pages(page_table, phdr->p_vaddr, buffer, phdr->p_filesz);
            
            kfree(buffer);
        }
        
        // 6. Zero-fill .bss (p_memsz > p_filesz)
        if (phdr->p_memsz > phdr->p_filesz) {
            uint64_t bss_start = phdr->p_vaddr + phdr->p_filesz;
            uint64_t bss_size = phdr->p_memsz - phdr->p_filesz;
            
            zero_user_memory(page_table, bss_start, bss_size);
        }
        
        return 0;
    }

Memory Layout
~~~~~~~~~~~~~

User process virtual memory layout:

.. code-block:: text

    0xFFFFFFFF_FFFFFFFF  ┌──────────────────────┐
                         │   Kernel Space       │
                         │   (not accessible    │
                         │    from user mode)   │
    0x80000000_00000000  ├──────────────────────┤
                         │                      │
                         │   (Unmapped)         │
                         │                      │
    0x00008000_00000000  ├──────────────────────┤
                         │   User Stack         │ ← Stack grows down
                         │   (8 KB)             │
    0x00007FFF_E0002000  ├──────────────────────┤
                         │                      │
                         │   (Unmapped)         │
                         │                      │
    0x00000000_00020000  ├──────────────────────┤
                         │   Heap (if used)     │ ← Grows up
    0x00000000_00018000  ├──────────────────────┤
                         │   .bss (zero init)   │
    0x00000000_00014000  ├──────────────────────┤
                         │   .data (init data)  │
    0x00000000_00012000  ├──────────────────────┤
                         │   .rodata (const)    │
    0x00000000_00011000  ├──────────────────────┤
                         │   .text (code)       │
    0x00000000_00010000  ├──────────────────────┤
                         │   (Unmapped)         │
    0x00000000_00000000  └──────────────────────┘

**Typical Addresses:**

- Code (.text): ``0x10000`` - ``0x11000``
- Data (.data): ``0x12000`` - ``0x14000``
- BSS (.bss): ``0x14000`` - ``0x18000``
- User Stack: ``0x7FFFE0000000`` - ``0x7FFFE0002000`` (8 KB)

User Stack Setup
~~~~~~~~~~~~~~~~

.. code-block:: c

    #define USER_STACK_TOP   0x7FFFE0002000
    #define USER_STACK_SIZE  (8 * 1024)  // 8 KB
    
    int allocate_user_stack(struct process *proc, uint64_t *page_table) {
        uint64_t stack_bottom = USER_STACK_TOP - USER_STACK_SIZE;
        
        // Allocate 2 pages for stack (8 KB)
        for (int i = 0; i < 2; i++) {
            uint64_t virt_addr = stack_bottom + (i * PAGE_SIZE);
            uint64_t phys_addr = pmm_alloc_page();
            
            if (phys_addr == 0) {
                return -1;
            }
            
            // Map as user-accessible, readable, writable (no execute)
            map_page(page_table, virt_addr, phys_addr,
                     PTE_VALID | PTE_USER | PTE_READ | PTE_WRITE);
            
            // Zero the page
            memset((void*)phys_to_virt(phys_addr), 0, PAGE_SIZE);
        }
        
        // Set stack pointer to top of stack
        proc->context.sp = USER_STACK_TOP;
        
        return 0;
    }

Page Table Creation
~~~~~~~~~~~~~~~~~~~

Each process gets its own page table for memory isolation:

.. code-block:: c

    uint64_t* create_page_table(void) {
        // 1. Allocate root page table
        uint64_t *page_table = (uint64_t*)pmm_alloc_page();
        if (page_table == NULL) {
            return NULL;
        }
        
        memset(page_table, 0, PAGE_SIZE);
        
        // 2. Map kernel space (higher half)
        // Copy kernel mappings from kernel page table
        uint64_t *kernel_pt = get_kernel_page_table();
        
        for (int i = 256; i < 512; i++) {  // Upper half
            page_table[i] = kernel_pt[i];
        }
        
        // 3. User space (lower half) is initially empty
        // Segments will be mapped during loading
        
        return page_table;
    }

Context Switching
~~~~~~~~~~~~~~~~~

When the process runs, its page table is activated:

.. code-block:: c

    void switch_to_process(struct process *proc) {
        // 1. Switch to process's page table
        uint64_t satp = make_satp(proc->page_table);
        write_csr(satp, satp);
        
        // 2. Flush TLB
        asm volatile("sfence.vma zero, zero");
        
        // 3. Restore process context (registers)
        restore_context(&proc->context);
        
        // 4. Switch to user mode and jump to PC
        sret();  // Returns to user mode at proc->context.pc
    }

Process Creation
~~~~~~~~~~~~~~~~

.. code-block:: c

    struct process* elf_create_process(const char *path) {
        // 1. Allocate process structure
        struct process *proc = process_alloc();
        if (!proc) {
            return NULL;
        }
        
        // 2. Set process name
        strncpy(proc->name, path, sizeof(proc->name) - 1);
        
        // 3. Create page table
        proc->page_table = create_page_table();
        if (!proc->page_table) {
            process_free(proc);
            return NULL;
        }
        
        // 4. Load ELF file
        if (elf_load_file(path, proc) != 0) {
            process_free(proc);
            return NULL;
        }
        
        // 5. Set up process state
        proc->state = PROCESS_READY;
        proc->privilege = PRIV_USER;  // User mode
        
        return proc;
    }

Error Handling
--------------

Common Errors
~~~~~~~~~~~~~

**File Not Found:**

.. code-block:: c

    int fd = vfs_open(path, O_RDONLY);
    if (fd < 0) {
        kprintf("ELF: Cannot open %s\n", path);
        return -1;
    }

**Invalid ELF:**

.. code-block:: c

    if (validate_elf_header(&header) != 0) {
        kprintf("ELF: Invalid ELF header in %s\n", path);
        return -1;
    }

**Out of Memory:**

.. code-block:: c

    uint64_t phys_addr = pmm_alloc_page();
    if (phys_addr == 0) {
        kprintf("ELF: Out of memory loading %s\n", path);
        cleanup_process(proc);
        return -1;
    }

**Wrong Architecture:**

.. code-block:: c

    if (header.e_machine != 0xF3) {  // RISC-V
        kprintf("ELF: Wrong architecture (expected RISC-V)\n");
        return -1;
    }

Security Considerations
-----------------------

Memory Protection
~~~~~~~~~~~~~~~~~

- **User pages are not executable** if not marked with ``PF_X``
- **Code pages are not writable** (no ``PF_W`` on code segment)
- **Kernel pages are inaccessible** from user mode (no ``PTE_USER`` flag)

Address Space Isolation
~~~~~~~~~~~~~~~~~~~~~~~~

Each process has its own page table:

- Process A cannot access Process B's memory
- Page faults occur on invalid memory access
- Kernel enforces permission bits (read/write/execute)

Stack Protection
~~~~~~~~~~~~~~~~

- Stack has guard pages (unmapped pages below stack)
- Stack overflow causes page fault
- No execute permission on stack (NX bit)

Limitations
-----------

Current Implementation
~~~~~~~~~~~~~~~~~~~~~~

- **No Dynamic Linking**: Only statically linked executables (``ET_EXEC``)
- **No Relocations**: Must be linked at fixed addresses
- **No Shared Libraries**: Cannot load ``.so`` files
- **No ASLR**: Processes always load at same virtual addresses
- **Fixed Stack Size**: 8 KB stack (no dynamic growth)
- **No Arguments/Environment**: Cannot pass argc/argv/envp

Future Enhancements
~~~~~~~~~~~~~~~~~~~

1. **Dynamic Linker Support**: Load and link ``.so`` files at runtime
2. **Position-Independent Executables (PIE)**: Support for ASLR
3. **Demand Paging**: Load pages on-demand (lazy loading)
4. **Copy-on-Write**: Share read-only pages between processes
5. **Program Arguments**: Pass command-line arguments to main()

Usage Examples
--------------

Creating an Executable
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: c

    // hello.c
    void _start(void) {
        const char *msg = "Hello from ELF!\n";
        asm volatile(
            "li a7, 1\n"        // SYS_WRITE
            "li a0, 1\n"        // stdout
            "mv a1, %0\n"       // buffer
            "li a2, 16\n"       // length
            "ecall\n"
            : : "r"(msg)
        );
        
        asm volatile(
            "li a7, 2\n"        // SYS_EXIT
            "li a0, 0\n"        // status
            "ecall\n"
        );
    }

**Compile:**

.. code-block:: bash

    riscv64-unknown-elf-gcc -nostdlib -static \
        -Wl,--entry=_start \
        -Wl,-Ttext=0x10000 \
        -o hello.elf hello.c

**Verify:**

.. code-block:: bash

    riscv64-unknown-elf-readelf -h hello.elf
    # Check: Entry point address: 0x10000
    # Check: Machine: RISC-V
    
    riscv64-unknown-elf-readelf -l hello.elf
    # Check: PT_LOAD segments

Loading from Shell
~~~~~~~~~~~~~~~~~~

.. code-block:: c

    // In ThunderOS shell
    ThunderOS> /bin/hello
    Hello from ELF!
    ThunderOS>

Internal shell command:

.. code-block:: c

    void shell_execute_program(const char *path) {
        // Create process from ELF
        struct process *proc = elf_create_process(path);
        
        if (!proc) {
            kprintf("Failed to load %s\n", path);
            return;
        }
        
        // Add to scheduler
        scheduler_add_process(proc);
        
        // Wait for completion
        int status;
        waitpid(proc->pid, &status, 0);
        
        kprintf("Process exited with status %d\n", status);
    }

Debugging
---------

ELF Inspection
~~~~~~~~~~~~~~

.. code-block:: bash

    # Dump ELF header
    riscv64-unknown-elf-readelf -h program.elf
    
    # List program headers
    riscv64-unknown-elf-readelf -l program.elf
    
    # Disassemble
    riscv64-unknown-elf-objdump -d program.elf
    
    # Show sections
    riscv64-unknown-elf-readelf -S program.elf

Runtime Tracing
~~~~~~~~~~~~~~~

.. code-block:: c

    #define ELF_DEBUG 1
    
    #if ELF_DEBUG
    #define ELF_LOG(fmt, ...) kprintf("[ELF] " fmt "\n", ##__VA_ARGS__)
    #else
    #define ELF_LOG(fmt, ...)
    #endif
    
    int load_segment(int fd, struct elf64_program_header *phdr, ...) {
        ELF_LOG("Loading segment:");
        ELF_LOG("  Type: 0x%x", phdr->p_type);
        ELF_LOG("  Offset: 0x%lx", phdr->p_offset);
        ELF_LOG("  VAddr: 0x%lx", phdr->p_vaddr);
        ELF_LOG("  FileSz: %lu", phdr->p_filesz);
        ELF_LOG("  MemSz: %lu", phdr->p_memsz);
        ELF_LOG("  Flags: 0x%x", phdr->p_flags);
        
        // ... loading code
    }

References
----------

- `ELF Specification <http://www.skyfree.org/linux/references/ELF_Format.pdf>`_
- `RISC-V ELF psABI <https://github.com/riscv-non-isa/riscv-elf-psabi-doc>`_
- `Linux ELF Loader <https://github.com/torvalds/linux/blob/master/fs/binfmt_elf.c>`_
- ThunderOS VFS: ``docs/source/internals/vfs.rst``
- ThunderOS processes: ``docs/source/internals/process.rst``

Implementation Files
--------------------

- ``kernel/core/elf_loader.c`` - ELF loader implementation
- ``include/kernel/elf.h`` - ELF data structures
- ``kernel/core/process.c`` - Process management
- ``kernel/mm/paging.c`` - Page table operations

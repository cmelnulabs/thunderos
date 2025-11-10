/*
 * RISC-V Sv39 Paging Implementation
 */

#include "mm/paging.h"
#include "mm/pmm.h"
#include "mm/kmalloc.h"
#include "hal/hal_uart.h"
#include "kernel/kstring.h"

// Kernel root page table (allocated statically for bootstrap)
static page_table_t kernel_page_table __attribute__((aligned(PAGE_SIZE)));

// Track if paging is enabled
static int paging_enabled = 0;

/**
 * Allocate a page table
 * Returns zeroed page table
 */
static page_table_t *alloc_page_table(void) {
    uintptr_t page = pmm_alloc_page();
    if (page == 0) {
        return NULL;
    }
    
    // Zero out the page table
    page_table_t *pt = (page_table_t *)page;
    for (int i = 0; i < PT_ENTRIES; i++) {
        pt->entries[i] = 0;
    }
    
    return pt;
}

/**
 * Walk page table and get PTE for virtual address
 * Creates intermediate tables if create=1
 */
static pte_t *walk_page_table(page_table_t *root, uintptr_t vaddr, int create) {
    page_table_t *pt = root;
    
    // Walk through 3 levels
    for (int level = 2; level > 0; level--) {
        // Get VPN for this level
        int vpn;
        if (level == 2) vpn = VPN_2(vaddr);
        else if (level == 1) vpn = VPN_1(vaddr);
        else vpn = VPN_0(vaddr);
        
        pte_t *pte = &pt->entries[vpn];
        
        if (*pte & PTE_V) {
            // Entry exists
            if (PTE_IS_LEAF(*pte)) {
                // Shouldn't have leaf at non-leaf level
                return NULL;
            }
            // Get next level page table
            uintptr_t next_pa = PTE_TO_PA(*pte);
            pt = (page_table_t *)next_pa;
        } else {
            // Entry doesn't exist
            if (!create) {
                return NULL;
            }
            
            // Allocate new page table for next level
            page_table_t *new_pt = alloc_page_table();
            if (new_pt == NULL) {
                return NULL;
            }
            
            // Create PTE pointing to new table (not a leaf)
            *pte = PA_TO_PTE((uintptr_t)new_pt, PTE_V);
            pt = new_pt;
        }
    }
    
    // Return pointer to leaf PTE (level 0)
    int vpn0 = VPN_0(vaddr);
    return &pt->entries[vpn0];
}

/**
 * Map a virtual address to physical address
 */
int map_page(page_table_t *page_table, uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    // Check alignment
    if ((vaddr & (PAGE_SIZE - 1)) || (paddr & (PAGE_SIZE - 1))) {
        hal_uart_puts("map_page: address not page-aligned\n");
        return -1;
    }
    
    // Walk page table (create intermediate tables)
    pte_t *pte = walk_page_table(page_table, vaddr, 1);
    if (pte == NULL) {
        hal_uart_puts("map_page: failed to walk page table\n");
        return -1;
    }
    
    // Check if already mapped
    if (*pte & PTE_V) {
        hal_uart_puts("map_page: address already mapped\n");
        return -1;
    }
    
    // Create mapping
    *pte = PA_TO_PTE(paddr, flags | PTE_V);
    
    return 0;
}

/**
 * Unmap a virtual address
 */
int unmap_page(page_table_t *page_table, uintptr_t vaddr) {
    // Check alignment
    if (vaddr & (PAGE_SIZE - 1)) {
        hal_uart_puts("unmap_page: address not page-aligned\n");
        return -1;
    }
    
    // Walk page table (don't create)
    pte_t *pte = walk_page_table(page_table, vaddr, 0);
    if (pte == NULL || !(*pte & PTE_V)) {
        hal_uart_puts("unmap_page: address not mapped\n");
        return -1;
    }
    
    // Clear PTE
    *pte = 0;
    
    // Flush TLB for this address
    tlb_flush(vaddr);
    
    return 0;
}

/**
 * Translate virtual address to physical address
 */
int virt_to_phys(page_table_t *page_table, uintptr_t vaddr, uintptr_t *paddr) {
    pte_t *pte = walk_page_table(page_table, vaddr, 0);
    if (pte == NULL || !(*pte & PTE_V)) {
        return -1;
    }
    
    // Get physical page number and add offset
    uintptr_t offset = vaddr & (PAGE_SIZE - 1);
    *paddr = PTE_TO_PA(*pte) + offset;
    
    return 0;
}

/**
 * Flush TLB
 */
void tlb_flush(uintptr_t vaddr) {
    if (vaddr == 0) {
        // Flush all TLB entries
        asm volatile("sfence.vma zero, zero" ::: "memory");
    } else {
        // Flush specific address
        asm volatile("sfence.vma %0, zero" :: "r"(vaddr) : "memory");
    }
}

/**
 * Get kernel page table
 */
page_table_t *get_kernel_page_table(void) {
    return &kernel_page_table;
}

/**
 * Enable paging by setting satp register
 */
static void enable_paging(page_table_t *root) {
    // Get physical address of root page table
    uintptr_t root_pa = (uintptr_t)root;
    
    // Build satp value: mode (Sv39) | ASID (0) | PPN
    uint64_t satp = SATP_MODE_SV39 | (root_pa >> SATP_PPN_SHIFT);
    
    // Set satp register
    asm volatile("csrw satp, %0" :: "r"(satp));
    
    // Flush TLB
    tlb_flush(0);
    
    paging_enabled = 1;
}

/**
 * Initialize paging
 */
void paging_init(uintptr_t kernel_start, uintptr_t kernel_end) {
    hal_uart_puts("Initializing virtual memory (Sv39)...\n");
    
    // Zero out kernel page table
    for (int i = 0; i < PT_ENTRIES; i++) {
        kernel_page_table.entries[i] = 0;
    }
    
    // Identity map kernel region (for early boot)
    // This allows us to continue executing after enabling paging
    hal_uart_puts("Identity mapping kernel: 0x");
    kprint_hex(kernel_start);
    hal_uart_puts(" - 0x");
    kprint_hex(kernel_end);
    hal_uart_puts("\n");
    
    uintptr_t addr = kernel_start & ~(PAGE_SIZE - 1);  // Align down
    uintptr_t end = (kernel_end + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);  // Align up
    
    while (addr < end) {
        if (map_page(&kernel_page_table, addr, addr, PTE_KERNEL_DATA | PTE_X) != 0) {
            hal_uart_puts("Failed to identity map kernel\n");
            return;
        }
        addr += PAGE_SIZE;
    }
    
    // Identity map all available RAM so PMM and kmalloc work
    // QEMU virt machine: 128MB from 0x80000000 to 0x88000000
    hal_uart_puts("Identity mapping all RAM (128MB)\n");
    
    uintptr_t ram_start = 0x80000000;
    uintptr_t ram_end = 0x88000000;
    
    addr = ram_start;
    while (addr < ram_end) {
        // Skip kernel pages (already mapped)
        if (addr >= kernel_start && addr < end) {
            addr += PAGE_SIZE;
            continue;
        }
        
        if (map_page(&kernel_page_table, addr, addr, PTE_KERNEL_DATA) != 0) {
            hal_uart_puts("Failed to identity map RAM at 0x");
            kprint_hex(addr);
            hal_uart_puts("\n");
            return;
        }
        addr += PAGE_SIZE;
    }
    
    // Map UART MMIO region (0x10000000)
    hal_uart_puts("Mapping UART MMIO\n");
    if (map_page(&kernel_page_table, 0x10000000, 0x10000000, PTE_KERNEL_DATA) != 0) {
        hal_uart_puts("Failed to map UART\n");
        return;
    }
    
    // Map CLINT MMIO region (0x2000000)
    hal_uart_puts("Mapping CLINT MMIO\n");
    if (map_page(&kernel_page_table, 0x2000000, 0x2000000, PTE_KERNEL_DATA) != 0) {
        hal_uart_puts("Failed to map CLINT\n");
        return;
    }
    
    // TODO: Map kernel to higher half (KERNEL_VIRT_BASE)
    // This requires updating linker script and entry point
    
    // Enable paging
    hal_uart_puts("Enabling paging...\n");
    enable_paging(&kernel_page_table);
    
    hal_uart_puts("Paging enabled successfully!\n");
    
    // Print stats
    hal_uart_puts("Virtual memory initialized (Sv39 mode)\n");
}

/**
 * Recursively free all page tables
 * 
 * @param pt Page table to free
 * @param level Current level (2 = root, 0 = leaf)
 */
static void free_page_table_recursive(page_table_t *pt, int level) {
    if (!pt) return;
    
    // For non-leaf levels, recursively free child page tables
    if (level > 0) {
        for (int i = 0; i < PT_ENTRIES; i++) {
            pte_t pte = pt->entries[i];
            
            // Skip invalid entries
            if (!(pte & PTE_V)) {
                continue;
            }
            
            // Skip leaf entries (they shouldn't exist at non-leaf levels in Sv39,
            // but check anyway to avoid corrupting memory)
            if (PTE_IS_LEAF(pte)) {
                continue;
            }
            
            // Get physical address of child page table
            uintptr_t child_pa = PTE_TO_PA(pte);
            page_table_t *child_pt = (page_table_t *)child_pa;
            
            // Recursively free child
            free_page_table_recursive(child_pt, level - 1);
        }
    }
    
    // Free this page table itself
    pmm_free_page((uintptr_t)pt);
}

/**
 * Free a page table and all its child page tables
 * 
 * This function walks the entire page table hierarchy and frees all
 * allocated page table pages. It does NOT free the actual data pages
 * mapped by the page table - those should be freed separately.
 * 
 * WARNING: Do not call this on the kernel page table!
 * 
 * @param page_table Root page table to free
 */
void free_page_table(page_table_t *page_table) {
    if (!page_table) return;
    
    // Don't free kernel page table
    if (page_table == &kernel_page_table) {
        hal_uart_puts("WARNING: Attempt to free kernel page table ignored\n");
        return;
    }
    
    // Recursively free all levels (starting at level 2)
    free_page_table_recursive(page_table, 2);
}

/**
 * Create a new page table for a user process
 * 
 * Creates an isolated virtual address space for a user process by:
 * 1. Allocating a new root page table
 * 2. Copying kernel memory mappings (VPN[2] = 2-511)
 * 3. Leaving user space empty (VPN[2] = 0-1)
 * 4. Mapping MMIO regions for kernel-mode syscall handling
 * 
 * The returned page table allows user code to execute with memory isolation,
 * while keeping kernel memory accessible for trap handling and system calls.
 * 
 * In Sv39, kernel space starts at VPN[2] = 2 (virtual address 0x80000000).
 * User space is VPN[2] = 0-1 (virtual address 0x00000000-0x7FFFFFFF).
 * 
 * @return Pointer to new page table, or NULL on failure
 */
page_table_t *create_user_page_table(void) {
    // Allocate a new page table structure from physical memory
    page_table_t *user_pt = alloc_page_table();
    if (user_pt == NULL) {
        return NULL;
    }
    
    // Copy kernel memory mappings from the global kernel page table
    // This allows supervisor code to access kernel memory even when running
    // in the user page table context (necessary for trap handling)
    // Kernel space: VPN[2] = 2 to 511 (covers 0x80000000 and above)
    for (int i = 2; i < PT_ENTRIES; i++) {
        user_pt->entries[i] = kernel_page_table.entries[i];
    }
    
    // Entries 0-1 (user space) remain zero/unmapped
    // This prevents user code from accessing uninitialized memory
    user_pt->entries[0] = 0;
    user_pt->entries[1] = 0;
    
    // Map UART MMIO region so supervisor mode can write to console
    // After switching to user page table, kernel trap handlers still need
    // to access UART for debug output and logging
    if (map_page(user_pt, 0x10000000, 0x10000000, PTE_KERNEL_DATA) != 0) {
        kfree(user_pt);
        return NULL;
    }
    
    // Map CLINT MMIO region for timer and interrupt handling
    // Supervisor mode needs this for IPI and scheduling timer management
    if (map_page(user_pt, 0x2000000, 0x2000000, PTE_KERNEL_DATA) != 0) {
        kfree(user_pt);
        return NULL;
    }
    
    return user_pt;
}

/**
 * Map user code into user address space
 * 
 * Allocates physical pages, copies user code from kernel memory, and creates
 * page table entries with user-executable permissions (R, X, U).
 * 
 * The user code is copied byte-by-byte from kernel memory to avoid requiring
 * temporary kernel mappings. Physical pages are zeroed before copying code.
 * 
 * @param page_table User process page table
 * @param user_vaddr Virtual address in user space (typically USER_CODE_BASE)
 * @param kernel_code Pointer to code in kernel memory to copy from
 * @param size Size of code in bytes
 * @return 0 on success, -1 on failure (allocation or page table error)
 */
int map_user_code(page_table_t *page_table, uintptr_t user_vaddr, 
                  void *kernel_code, size_t size) {
    if (!page_table || !kernel_code || size == 0) {
        return -1;
    }
    
    // Align virtual address down to page boundary
    uintptr_t vaddr = user_vaddr & ~(PAGE_SIZE - 1);
    // Calculate offset within first page
    uintptr_t offset = user_vaddr & (PAGE_SIZE - 1);
    // Total bytes needed including offset
    size_t total_size = size + offset;
    // Number of pages required
    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Map each page with user code
    uint8_t *src = (uint8_t *)kernel_code;
    size_t src_offset = 0;  // Track total source bytes copied
    
    for (size_t i = 0; i < num_pages; i++) {
        // Allocate physical page from physical memory manager
        uintptr_t phys_page = pmm_alloc_page();
        if (phys_page == 0) {
            // TODO: Clean up previously allocated pages
            return -1;
        }
        
        // Zero the page first (security: clear any old data)
        uint8_t *page_ptr = (uint8_t *)phys_page;
        for (size_t j = 0; j < PAGE_SIZE; j++) {
            page_ptr[j] = 0;
        }
        
        // Determine how many bytes to copy to this page
        size_t copy_size = PAGE_SIZE;
        uint8_t *copy_dest = page_ptr;
        
        // First page: skip offset bytes, then copy
        if (i == 0 && offset > 0) {
            copy_dest += offset;
            copy_size = PAGE_SIZE - offset;
        }
        
        // Last page: may be partial, only copy remaining bytes
        if (i == num_pages - 1) {
            size_t remaining = size + offset - src_offset;
            if (remaining < copy_size) {
                copy_size = remaining;
            }
        }
        
        // Copy code bytes from kernel memory to physical page
        for (size_t j = 0; j < copy_size; j++) {
            copy_dest[j] = src[src_offset + j];
        }
        
        // Update source offset for next page
        src_offset += copy_size;
        
        // Create page table entry with user-executable permissions
        uintptr_t map_vaddr = vaddr + (i * PAGE_SIZE);
        if (map_page(page_table, map_vaddr, phys_page, PTE_USER_TEXT) != 0) {
            pmm_free_page(phys_page);
            return -1;
        }
    }
    
    return 0;
}

/**
 * Map user memory (stack, heap, data) into user address space
 * 
 * Maps one or more pages into user address space with permissions based on usage.
 * Can either allocate new anonymous pages or map pre-allocated physical pages.
 * 
 * @param page_table User process page table
 * @param user_vaddr Starting virtual address in user space
 * @param phys_addr Physical address to map (0 = allocate new pages)
 * @param size Total size in bytes to map
 * @param writable 1 for read-write (PTE_USER_DATA), 0 for read-only (PTE_USER_RO)
 * @return 0 on success, -1 on failure (allocation or page table error)
 */
int map_user_memory(page_table_t *page_table, uintptr_t user_vaddr, 
                    uintptr_t phys_addr, size_t size, int writable) {
    if (!page_table || size == 0) {
        return -1;
    }
    
    // Align virtual address down to page boundary
    uintptr_t vaddr = user_vaddr & ~(PAGE_SIZE - 1);
    // Number of pages needed
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Choose permissions based on writability requirement
    uint64_t flags = writable ? PTE_USER_DATA : PTE_USER_RO;
    
    // Determine if we should allocate new pages or use provided physical address
    int allocate_pages = (phys_addr == 0);
    
    // Track allocated pages for cleanup on failure
    uintptr_t *allocated_pages = NULL;
    size_t allocated_count = 0;
    
    if (allocate_pages) {
        // Allocate array to track pages we allocate (for cleanup on failure)
        allocated_pages = (uintptr_t *)kmalloc(num_pages * sizeof(uintptr_t));
        if (!allocated_pages) {
            return -1;
        }
    }
    
    // Map each page
    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t phys_page;
        
        if (allocate_pages) {
            // Allocate new anonymous page from physical memory manager
            phys_page = pmm_alloc_page();
            if (phys_page == 0) {
                // Free all previously allocated pages
                for (size_t j = 0; j < allocated_count; j++) {
                    pmm_free_page(allocated_pages[j]);
                }
                kfree(allocated_pages);
                return -1;
            }
            
            // Track this allocation for potential cleanup
            allocated_pages[allocated_count++] = phys_page;
            
            // Zero the page for security (prevent information leakage)
            uint8_t *page_ptr = (uint8_t *)phys_page;
            for (size_t j = 0; j < PAGE_SIZE; j++) {
                page_ptr[j] = 0;
            }
        } else {
            // Use provided physical address
            phys_page = phys_addr + (i * PAGE_SIZE);
        }
        
        // Create page table entry for this virtual-to-physical mapping
        uintptr_t map_vaddr = vaddr + (i * PAGE_SIZE);
        if (map_page(page_table, map_vaddr, phys_page, flags) != 0) {
            // Free all allocated pages on failure
            if (allocate_pages) {
                for (size_t j = 0; j < allocated_count; j++) {
                    pmm_free_page(allocated_pages[j]);
                }
                kfree(allocated_pages);
            }
            return -1;
        }
    }
    
    // Cleanup tracking array (pages are now mapped, so don't free them)
    if (allocate_pages) {
        kfree(allocated_pages);
    }
    
    return 0;
}

/**
 * Switch to a different page table
 * 
 * Updates the satp register and flushes the TLB.
 * 
 * @param page_table New page table to switch to
 */
void switch_page_table(page_table_t *page_table) {
    if (!page_table) {
        hal_uart_puts("switch_page_table: NULL page table\n");
        return;
    }
    
    // Get physical address of root page table
    uintptr_t root_pa = (uintptr_t)page_table;
    
    // Build satp value: mode (Sv39) | ASID (0) | PPN
    uint64_t satp = SATP_MODE_SV39 | (root_pa >> SATP_PPN_SHIFT);
    
    // Set satp register
    asm volatile("csrw satp, %0" :: "r"(satp) : "memory");
    
    // Flush entire TLB
    tlb_flush(0);
}

/**
 * Translate virtual to physical using kernel page table
 * 
 * Convenience wrapper for common case of translating kernel virtual addresses.
 */
uintptr_t translate_virt_to_phys(uintptr_t vaddr) {
    uintptr_t paddr;
    
    // Try using page table translation
    if (virt_to_phys(&kernel_page_table, vaddr, &paddr) == 0) {
        return paddr;
    }
    
    // Fallback: assume identity mapping (current kernel design)
    return vaddr;
}

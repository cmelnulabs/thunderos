/*
 * RISC-V Sv39 Paging Implementation
 */

#include "mm/paging.h"
#include "mm/pmm.h"
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
 * Creates a new page table with kernel mappings but no user mappings.
 * The user space (low memory) is left empty for user code/data.
 * 
 * In Sv39, virtual addresses are divided:
 * - 0x00000000_00000000 to 0x00000000_3FFFFFFF: User space (VPN[2] = 0-1)
 * - 0x00000000_80000000 to 0xFFFFFFFF_FFFFFFFF: Kernel space (VPN[2] = 2-511)
 * 
 * We copy only the kernel entries (VPN[2] = 2 to 511) from the kernel page table.
 * 
 * @return Pointer to new page table, or NULL on failure
 */
page_table_t *create_user_page_table(void) {
    // Allocate root page table
    page_table_t *user_pt = alloc_page_table();
    if (user_pt == NULL) {
        hal_uart_puts("create_user_page_table: failed to allocate page table\n");
        return NULL;
    }
    
    // Copy kernel mappings from kernel page table
    // In Sv39, kernel space starts at 0x80000000 which is VPN[2] = 2
    // Copy entries 2-511 (kernel space)
    for (int i = 2; i < PT_ENTRIES; i++) {
        user_pt->entries[i] = kernel_page_table.entries[i];
    }
    
    // Entries 0-1 (user space) remain zero/unmapped
    user_pt->entries[0] = 0;
    user_pt->entries[1] = 0;
    
    return user_pt;
}

/**
 * Map user code into user address space
 * 
 * Allocates physical pages, copies code from kernel space, and maps
 * with user-executable permissions.
 * 
 * @param page_table User process page table
 * @param user_vaddr Virtual address in user space (e.g., USER_CODE_BASE)
 * @param kernel_code Pointer to code in kernel memory to copy
 * @param size Size of code in bytes
 * @return 0 on success, -1 on failure
 */
int map_user_code(page_table_t *page_table, uintptr_t user_vaddr, 
                  void *kernel_code, size_t size) {
    if (!page_table || !kernel_code || size == 0) {
        hal_uart_puts("map_user_code: invalid parameters\n");
        return -1;
    }
    
    // Align to page boundaries
    uintptr_t vaddr = user_vaddr & ~(PAGE_SIZE - 1);
    uintptr_t offset = user_vaddr & (PAGE_SIZE - 1);
    size_t total_size = size + offset;
    size_t num_pages = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    hal_uart_puts("map_user_code: mapping ");
    kprint_dec(num_pages);
    hal_uart_puts(" pages at 0x");
    kprint_hex(user_vaddr);
    hal_uart_puts("\n");
    
    // Map each page
    uint8_t *src = (uint8_t *)kernel_code;
    for (size_t i = 0; i < num_pages; i++) {
        // Allocate physical page
        uintptr_t phys_page = pmm_alloc_page();
        if (phys_page == 0) {
            hal_uart_puts("map_user_code: failed to allocate physical page\n");
            // TODO: Free previously allocated pages
            return -1;
        }
        
        // Zero the page first
        uint8_t *page_ptr = (uint8_t *)phys_page;
        for (size_t j = 0; j < PAGE_SIZE; j++) {
            page_ptr[j] = 0;
        }
        
        // Copy code to physical page
        size_t copy_size = PAGE_SIZE;
        
        // Handle first page with offset
        if (i == 0 && offset > 0) {
            page_ptr += offset;
            copy_size = PAGE_SIZE - offset;
        }
        
        // Handle last page (might be partial)
        if (i == num_pages - 1) {
            size_t remaining = size - (i * PAGE_SIZE);
            if (i == 0) {
                remaining += offset;
            }
            if (remaining < copy_size) {
                copy_size = remaining;
            }
        }
        
        // Copy data
        for (size_t j = 0; j < copy_size; j++) {
            page_ptr[j] = src[i * PAGE_SIZE + j];
        }
        
        // Map the page with user-executable permissions
        uintptr_t map_vaddr = vaddr + (i * PAGE_SIZE);
        if (map_page(page_table, map_vaddr, phys_page, PTE_USER_TEXT) != 0) {
            hal_uart_puts("map_user_code: failed to map page at 0x");
            kprint_hex(map_vaddr);
            hal_uart_puts("\n");
            pmm_free_page(phys_page);
            return -1;
        }
    }
    
    return 0;
}

/**
 * Map user memory (stack, heap, data) into user address space
 * 
 * @param page_table User process page table
 * @param user_vaddr Virtual address in user space
 * @param phys_addr Physical address (0 = allocate new pages)
 * @param size Size in bytes
 * @param writable 1 if writable, 0 if read-only
 * @return 0 on success, -1 on failure
 */
int map_user_memory(page_table_t *page_table, uintptr_t user_vaddr, 
                    uintptr_t phys_addr, size_t size, int writable) {
    if (!page_table || size == 0) {
        hal_uart_puts("map_user_memory: invalid parameters\n");
        return -1;
    }
    
    // Align to page boundaries
    uintptr_t vaddr = user_vaddr & ~(PAGE_SIZE - 1);
    size_t num_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    hal_uart_puts("map_user_memory: mapping ");
    kprint_dec(num_pages);
    hal_uart_puts(" pages at 0x");
    kprint_hex(user_vaddr);
    hal_uart_puts(writable ? " (RW)\n" : " (RO)\n");
    
    // Choose permissions based on writable flag
    uint64_t flags = writable ? PTE_USER_DATA : PTE_USER_RO;
    
    // Map each page
    int allocate_pages = (phys_addr == 0);
    for (size_t i = 0; i < num_pages; i++) {
        uintptr_t phys_page;
        
        if (allocate_pages) {
            // Allocate new physical page
            phys_page = pmm_alloc_page();
            if (phys_page == 0) {
                hal_uart_puts("map_user_memory: failed to allocate physical page\n");
                return -1;
            }
            
            // Zero the page
            uint8_t *page_ptr = (uint8_t *)phys_page;
            for (size_t j = 0; j < PAGE_SIZE; j++) {
                page_ptr[j] = 0;
            }
        } else {
            // Use provided physical address
            phys_page = phys_addr + (i * PAGE_SIZE);
        }
        
        // Map the page
        uintptr_t map_vaddr = vaddr + (i * PAGE_SIZE);
        if (map_page(page_table, map_vaddr, phys_page, flags) != 0) {
            hal_uart_puts("map_user_memory: failed to map page at 0x");
            kprint_hex(map_vaddr);
            hal_uart_puts("\n");
            if (allocate_pages) {
                pmm_free_page(phys_page);
            }
            return -1;
        }
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


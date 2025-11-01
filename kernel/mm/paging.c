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


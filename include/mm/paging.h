/*
 * RISC-V Sv39 Paging Support
 * 
 * Sv39 provides 39-bit virtual addresses with 3-level page tables:
 * - Level 2 (root): VPN[2] - 512 entries
 * - Level 1: VPN[1] - 512 entries  
 * - Level 0: VPN[0] - 512 entries
 * - Page size: 4KB
 */

#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>
#include <stddef.h>

// Page size: 4KB (same as PMM)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Virtual address layout (Sv39)
#define VPN_SHIFT 12
#define VPN_MASK 0x1FF  // 9 bits per level
#define SATP_PPN_SHIFT 12

// Extract VPN (Virtual Page Number) from virtual address
#define VPN_0(va) (((va) >> 12) & VPN_MASK)  // Level 0
#define VPN_1(va) (((va) >> 21) & VPN_MASK)  // Level 1
#define VPN_2(va) (((va) >> 30) & VPN_MASK)  // Level 2

// Page table entry flags (bits 0-7)
#define PTE_V    (1 << 0)  // Valid
#define PTE_R    (1 << 1)  // Readable
#define PTE_W    (1 << 2)  // Writable
#define PTE_X    (1 << 3)  // Executable
#define PTE_U    (1 << 4)  // User accessible
#define PTE_G    (1 << 5)  // Global mapping
#define PTE_A    (1 << 6)  // Accessed
#define PTE_D    (1 << 7)  // Dirty

// Common permission combinations
#define PTE_KERNEL_TEXT  (PTE_V | PTE_R | PTE_X)           // Kernel code
#define PTE_KERNEL_DATA  (PTE_V | PTE_R | PTE_W)           // Kernel data
#define PTE_KERNEL_RODATA (PTE_V | PTE_R)                  // Kernel read-only
#define PTE_USER_TEXT    (PTE_V | PTE_R | PTE_X | PTE_U)   // User code
#define PTE_USER_DATA    (PTE_V | PTE_R | PTE_W | PTE_U)   // User data/stack
#define PTE_USER_RO      (PTE_V | PTE_R | PTE_U)           // User read-only

// SATP register mode (bits 60-63)
#define SATP_MODE_SV39  (8UL << 60)

// Page table entry structure
typedef uint64_t pte_t;

// Page table (512 entries per table)
#define PT_ENTRIES 512
typedef struct {
    pte_t entries[PT_ENTRIES];
} page_table_t;

// Extract physical page number from PTE
#define PTE_TO_PPN(pte) (((pte) >> 10) & 0xFFFFFFFFFFFUL)
#define PTE_TO_PA(pte)  (PTE_TO_PPN(pte) << PAGE_SHIFT)

// Create PTE from physical address and flags
#define PA_TO_PTE(pa, flags) ((((pa) >> PAGE_SHIFT) << 10) | (flags))

// Check if PTE is a leaf (points to physical page)
#define PTE_IS_LEAF(pte) ((pte) & (PTE_R | PTE_W | PTE_X))

// Virtual memory layout
#define KERNEL_VIRT_BASE  0xFFFFFFFF80000000UL  // -2GB (higher half)
#define USER_VIRT_BASE    0x0000000000000000UL  // 0GB (lower half)
#define USER_VIRT_END     0x0000004000000000UL  // 256GB max user space

/**
 * Initialize virtual memory system
 * 
 * Sets up page tables and enables paging via satp register.
 * 
 * @param kernel_start Physical address of kernel start
 * @param kernel_end Physical address of kernel end
 */
void paging_init(uintptr_t kernel_start, uintptr_t kernel_end);

/**
 * Map a virtual address to a physical address
 * 
 * @param page_table Root page table (level 2)
 * @param vaddr Virtual address (must be page-aligned)
 * @param paddr Physical address (must be page-aligned)
 * @param flags PTE flags (permissions)
 * @return 0 on success, -1 on failure
 */
int map_page(page_table_t *page_table, uintptr_t vaddr, uintptr_t paddr, uint64_t flags);

/**
 * Unmap a virtual address
 * 
 * @param page_table Root page table (level 2)
 * @param vaddr Virtual address (must be page-aligned)
 * @return 0 on success, -1 on failure
 */
int unmap_page(page_table_t *page_table, uintptr_t vaddr);

/**
 * Translate virtual address to physical address
 * 
 * @param page_table Root page table (level 2)
 * @param vaddr Virtual address
 * @param paddr Output: physical address
 * @return 0 on success, -1 if not mapped
 */
int virt_to_phys(page_table_t *page_table, uintptr_t vaddr, uintptr_t *paddr);

/**
 * Flush TLB for a specific virtual address
 * 
 * @param vaddr Virtual address to flush (0 = flush all)
 */
void tlb_flush(uintptr_t vaddr);

/**
 * Get the kernel's root page table
 * 
 * @return Pointer to kernel page table
 */
page_table_t *get_kernel_page_table(void);

/**
 * Create a new page table for a user process
 * 
 * Creates a new page table with kernel mappings but no user mappings.
 * The user space (low memory) is left empty for user code/data.
 * 
 * @return Pointer to new page table, or NULL on failure
 */
page_table_t *create_user_page_table(void);

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
                  void *kernel_code, size_t size);

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
                    uintptr_t phys_addr, size_t size, int writable);

/**
 * Switch to a different page table
 * 
 * Updates the satp register and flushes the TLB.
 * 
 * @param page_table New page table to switch to
 */
void switch_page_table(page_table_t *page_table);

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
void free_page_table(page_table_t *page_table);

/**
 * Convert kernel virtual address to physical address
 * (Assumes higher-half kernel mapping)
 * 
 * @param vaddr Kernel virtual address
 * @return Physical address
 */
static inline uintptr_t kernel_virt_to_phys(uintptr_t vaddr) {
    if (vaddr >= KERNEL_VIRT_BASE) {
        return vaddr - KERNEL_VIRT_BASE;
    }
    // Identity mapped (early boot)
    return vaddr;
}

/**
 * Convert physical address to kernel virtual address
 * (Assumes higher-half kernel mapping)
 * 
 * @param paddr Physical address
 * @return Kernel virtual address
 */
static inline uintptr_t kernel_phys_to_virt(uintptr_t paddr) {
    return paddr + KERNEL_VIRT_BASE;
}

/**
 * Translate virtual to physical using current page table
 * 
 * Convenience wrapper that uses the kernel page table.
 * For user page tables, use virt_to_phys() directly.
 * 
 * @param vaddr Virtual address
 * @return Physical address, or 0 if not mapped
 */
uintptr_t translate_virt_to_phys(uintptr_t vaddr);

/**
 * Translate physical to virtual (identity mapping assumption)
 * 
 * In our current kernel design, all physical RAM is identity-mapped,
 * so physical address = virtual address for kernel space.
 * 
 * @param paddr Physical address
 * @return Virtual address
 */
static inline uintptr_t translate_phys_to_virt(uintptr_t paddr) {
    // Currently using identity mapping for kernel
    return paddr;
}

#endif // PAGING_H

/*
 * Physical Memory Manager (PMM)
 * 
 * Manages physical memory allocation at page granularity.
 * Uses a bitmap to track which 4KB pages are free or allocated.
 */

#ifndef PMM_H
#define PMM_H

#include <stddef.h>
#include <stdint.h>

// Page size: 4KB (standard for RISC-V Sv39)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Align address down to page boundary
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

// Align address up to page boundary
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

// Convert between physical addresses and page numbers
#define ADDR_TO_PAGE(addr) ((addr) >> PAGE_SHIFT)
#define PAGE_TO_ADDR(page) ((page) << PAGE_SHIFT)

/**
 * Initialize the physical memory manager
 * 
 * @param mem_start Start of usable physical memory (after kernel)
 * @param mem_size Total size of usable memory in bytes
 */
void pmm_init(uintptr_t mem_start, size_t mem_size);

/**
 * Allocate a single physical page
 * 
 * @return Physical address of allocated page, or 0 if out of memory
 */
uintptr_t pmm_alloc_page(void);

/**
 * Allocate multiple contiguous physical pages
 * 
 * @param num_pages Number of contiguous pages to allocate
 * @return Physical address of first allocated page, or 0 if unable to allocate
 */
uintptr_t pmm_alloc_pages(size_t num_pages);

/**
 * Free a previously allocated physical page
 * 
 * @param page_addr Physical address of page to free (must be page-aligned)
 */
void pmm_free_page(uintptr_t page_addr);

/**
 * Free multiple contiguous physical pages
 * 
 * @param page_addr Physical address of first page to free (must be page-aligned)
 * @param num_pages Number of contiguous pages to free
 */
void pmm_free_pages(uintptr_t page_addr, size_t num_pages);

/**
 * Get memory statistics
 * 
 * @param total_pages Output: total number of pages managed
 * @param free_pages Output: number of free pages available
 */
void pmm_get_stats(size_t *total_pages, size_t *free_pages);

#endif // PMM_H

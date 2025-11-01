/*
 * Kernel Memory Allocator Implementation
 * 
 * Simple page-based allocator for now.
 * For allocations < PAGE_SIZE, we allocate a full page (wasteful but simple).
 * Future: Implement slab allocator for small objects.
 */

#include "mm/kmalloc.h"
#include "mm/pmm.h"
#include "kernel/panic.h"
#include "hal/hal_uart.h"

// Allocation header (stored at start of each allocation)
struct kmalloc_header {
    size_t size;           // Size of allocation in bytes
    size_t pages;          // Number of pages allocated
    unsigned int magic;    // Magic number for validation
};

#define KMALLOC_MAGIC 0xDEADBEEF
#define HEADER_SIZE sizeof(struct kmalloc_header)

/**
 * Allocate kernel memory
 */
void *kmalloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    
    // Calculate total size including header
    size_t total_size = size + HEADER_SIZE;
    
    // Calculate number of pages needed
    size_t pages_needed = (total_size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    // Allocate page(s)
    uintptr_t page_addr;
    if (pages_needed == 1) {
        page_addr = pmm_alloc_page();
    } else {
        page_addr = pmm_alloc_pages(pages_needed);
    }
    
    if (page_addr == 0) {
        return NULL;
    }
    
    // Set up allocation header
    struct kmalloc_header *header = (struct kmalloc_header *)page_addr;
    header->size = size;
    header->pages = pages_needed;
    header->magic = KMALLOC_MAGIC;
    
    // Return pointer after header
    return (void *)(page_addr + HEADER_SIZE);
}

/**
 * Free kernel memory
 */
void kfree(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    
    // Get header
    struct kmalloc_header *header = (struct kmalloc_header *)((uintptr_t)ptr - HEADER_SIZE);
    
    // Validate magic number
    if (header->magic != KMALLOC_MAGIC) {
        kernel_panic("kfree: Invalid pointer or corrupted heap header");
    }
    
    // Free pages
    uintptr_t page_addr = (uintptr_t)header;
    if (header->pages == 1) {
        pmm_free_page(page_addr);
    } else {
        pmm_free_pages(page_addr, header->pages);
    }
}

/**
 * Allocate aligned kernel memory
 */
void *kmalloc_aligned(size_t size, size_t align) {
    // For now, just use kmalloc (pages are already 4KB aligned)
    // TODO: Implement proper alignment support
    if (align <= PAGE_SIZE) {
        return kmalloc(size);
    }
    
    hal_uart_puts("kmalloc_aligned: Alignment > PAGE_SIZE not yet supported\n");
    return NULL;
}

/*
 * DMA (Direct Memory Access) Allocator Implementation
 */

#include "mm/dma.h"
#include "mm/pmm.h"
#include "mm/paging.h"
#include "mm/kmalloc.h"
#include "hal/hal_uart.h"
#include "kernel/kstring.h"

// Linked list of allocated DMA regions for tracking
static dma_region_t *dma_regions_head = NULL;

// Statistics
static size_t total_regions = 0;
static size_t total_bytes = 0;

/**
 * Initialize the DMA allocator
 */
void dma_init(void) {
    hal_uart_puts("Initializing DMA allocator...\n");
    dma_regions_head = NULL;
    total_regions = 0;
    total_bytes = 0;
    hal_uart_puts("DMA allocator initialized\n");
}

/**
 * Allocate a DMA-capable memory region
 * 
 * Strategy:
 * 1. Allocate contiguous physical pages from PMM
 * 2. Physical pages are already identity-mapped in kernel space
 * 3. Create DMA region structure to track both addresses
 */
dma_region_t *dma_alloc(size_t size, uint32_t flags) {
    if (size == 0) {
        return NULL;
    }
    
    // Round size up to page boundary
    size_t aligned_size = (size + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
    size_t num_pages = aligned_size / PAGE_SIZE;
    
    // Allocate physically contiguous pages
    uintptr_t phys_addr = pmm_alloc_pages(num_pages);
    if (phys_addr == 0) {
        hal_uart_puts("dma_alloc: failed to allocate physical pages\n");
        return NULL;
    }
    
    // In our current identity-mapped kernel, virtual address = physical address
    // This works because paging_init() identity maps all RAM
    void *virt_addr = (void *)phys_addr;
    
    // Zero memory if requested
    if (flags & DMA_ZERO) {
        uint8_t *ptr = (uint8_t *)virt_addr;
        for (size_t i = 0; i < aligned_size; i++) {
            ptr[i] = 0;
        }
    }
    
    // Create DMA region structure (allocate from kernel heap)
    dma_region_t *region = (dma_region_t *)kmalloc(sizeof(dma_region_t));
    if (region == NULL) {
        // Free the physical pages and return
        pmm_free_pages(phys_addr, num_pages);
        hal_uart_puts("dma_alloc: failed to allocate region structure\n");
        return NULL;
    }
    
    // Fill in region information
    region->virt_addr = virt_addr;
    region->phys_addr = phys_addr;
    region->size = aligned_size;
    region->next = NULL;
    
    // Add to linked list for tracking
    if (dma_regions_head == NULL) {
        dma_regions_head = region;
    } else {
        // Add to end of list
        dma_region_t *current = dma_regions_head;
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = region;
    }
    
    // Update statistics
    total_regions++;
    total_bytes += aligned_size;
    
    return region;
}

/**
 * Free a DMA region
 */
void dma_free(dma_region_t *region) {
    if (region == NULL) {
        return;
    }
    
    // Calculate number of pages to free
    size_t num_pages = region->size / PAGE_SIZE;
    
    // Free physical pages
    pmm_free_pages(region->phys_addr, num_pages);
    
    // Remove from linked list
    if (dma_regions_head == region) {
        // First element
        dma_regions_head = region->next;
    } else {
        // Find previous element
        dma_region_t *current = dma_regions_head;
        while (current != NULL && current->next != region) {
            current = current->next;
        }
        if (current != NULL) {
            current->next = region->next;
        }
    }
    
    // Update statistics
    total_regions--;
    total_bytes -= region->size;
    
    // Free the region structure itself
    kfree(region);
}

/**
 * Get DMA statistics
 */
void dma_get_stats(size_t *allocated_regions, size_t *allocated_bytes) {
    if (allocated_regions != NULL) {
        *allocated_regions = total_regions;
    }
    if (allocated_bytes != NULL) {
        *allocated_bytes = total_bytes;
    }
}

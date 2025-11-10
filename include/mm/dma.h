/*
 * DMA (Direct Memory Access) Allocator
 * 
 * Provides allocation of physically contiguous memory regions for device I/O.
 * DMA requires physical addresses and guarantees physical contiguity across
 * multiple pages, which the standard PMM and kmalloc cannot provide reliably.
 */

#ifndef DMA_H
#define DMA_H

#include <stddef.h>
#include <stdint.h>

/**
 * DMA allocation flags
 */
#define DMA_ZERO        (1 << 0)  // Zero the allocated memory
#define DMA_ALIGN_4K    (1 << 1)  // Align to 4KB (page boundary)
#define DMA_ALIGN_64K   (1 << 2)  // Align to 64KB (for some devices)

/**
 * DMA region structure
 * 
 * Represents a physically contiguous memory region for DMA.
 * Tracks both virtual and physical addresses.
 */
typedef struct dma_region {
    void *virt_addr;          // Virtual address (kernel can access)
    uintptr_t phys_addr;      // Physical address (for device)
    size_t size;              // Size in bytes
    struct dma_region *next;  // Linked list for tracking
} dma_region_t;

/**
 * Initialize the DMA allocator
 * 
 * Must be called after PMM and paging are initialized.
 */
void dma_init(void);

/**
 * Allocate a DMA-capable memory region
 * 
 * Allocates physically contiguous memory suitable for device I/O.
 * Returns both virtual and physical addresses.
 * 
 * @param size Size in bytes (will be rounded up to page size)
 * @param flags Allocation flags (DMA_ZERO, DMA_ALIGN_4K, etc.)
 * @return Pointer to DMA region structure, or NULL on failure
 */
dma_region_t *dma_alloc(size_t size, uint32_t flags);

/**
 * Free a DMA region
 * 
 * @param region DMA region to free (returned by dma_alloc)
 */
void dma_free(dma_region_t *region);

/**
 * Get physical address from DMA region
 * 
 * @param region DMA region
 * @return Physical address
 */
static inline uintptr_t dma_phys_addr(dma_region_t *region) {
    return region ? region->phys_addr : 0;
}

/**
 * Get virtual address from DMA region
 * 
 * @param region DMA region
 * @return Virtual address (can be dereferenced)
 */
static inline void *dma_virt_addr(dma_region_t *region) {
    return region ? region->virt_addr : NULL;
}

/**
 * Get size of DMA region
 * 
 * @param region DMA region
 * @return Size in bytes
 */
static inline size_t dma_size(dma_region_t *region) {
    return region ? region->size : 0;
}

/**
 * Get DMA statistics
 * 
 * @param allocated_regions Output: number of currently allocated regions
 * @param allocated_bytes Output: total bytes allocated
 */
void dma_get_stats(size_t *allocated_regions, size_t *allocated_bytes);

#endif // DMA_H

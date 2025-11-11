/*
 * VirtIO Block Device Driver
 * 
 * Implementation of VirtIO 1.0+ block device driver using MMIO interface.
 * Provides synchronous block I/O with DMA-allocated buffers.
 */

#include <drivers/virtio_blk.h>
#include <mm/dma.h>
#include <mm/paging.h>
#include <mm/kmalloc.h>
#include <arch/barrier.h>
#include <hal/hal_uart.h>
#include <stddef.h>
#include <stdint.h>

/* Helper macros for MMIO register access */
#define VIRTIO_READ32(dev, offset) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))))

#define VIRTIO_WRITE32(dev, offset, value) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))) = (value))

/* Global device state */
static virtio_blk_device_t *g_blk_device = NULL;

/* Forward declarations */
static int virtqueue_init(virtio_blk_device_t *dev, uint32_t queue_size);
static int virtqueue_alloc_desc_chain(virtqueue_t *vq, uint16_t *desc_idx, uint32_t count);
static void virtqueue_free_desc_chain(virtqueue_t *vq, uint16_t desc_idx);
static void virtqueue_add_to_avail(virtqueue_t *vq, uint16_t desc_idx);
static int virtqueue_get_used_buf(virtqueue_t *vq, uint16_t *desc_idx, uint32_t *len);
static void virtqueue_notify(virtio_blk_device_t *dev, uint32_t queue_idx);

/**
 * Initialize virtqueue with descriptor, available, and used rings
 */
static int virtqueue_init(virtio_blk_device_t *dev, uint32_t queue_size)
{
    virtqueue_t *vq = &dev->queue;
    vq->queue_size = queue_size;
    vq->last_seen_used = 0;
    vq->num_free = queue_size;
    
    /* Calculate sizes for each ring */
    size_t desc_size = sizeof(virtq_desc_t) * queue_size;
    size_t avail_size = sizeof(uint16_t) * (3 + queue_size);
    size_t used_size = sizeof(uint16_t) * 3 + sizeof(virtq_used_elem_t) * queue_size;
    
    /* Allocate descriptor ring using DMA allocator */
    dma_region_t *desc_region = dma_alloc(desc_size, DMA_ZERO);
    if (!desc_region) {
        return -1;
    }
    vq->desc = (virtq_desc_t *)desc_region->virt_addr;
    vq->desc_phys = desc_region->phys_addr;
    
    /* Allocate available ring */
    dma_region_t *avail_region = dma_alloc(avail_size, DMA_ZERO);
    if (!avail_region) {
        dma_free(desc_region);
        return -1;
    }
    vq->avail = (virtq_avail_t *)avail_region->virt_addr;
    vq->avail_phys = avail_region->phys_addr;
    
    /* Allocate used ring */
    dma_region_t *used_region = dma_alloc(used_size, DMA_ZERO);
    if (!used_region) {
        dma_free(desc_region);
        dma_free(avail_region);
        return -1;
    }
    vq->used = (virtq_used_t *)used_region->virt_addr;
    vq->used_phys = used_region->phys_addr;
    
    /* Initialize free descriptor list (link all descriptors together) */
    for (uint16_t i = 0; i < queue_size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[queue_size - 1].next = 0;
    vq->free_head = 0;
    
    /* Configure queue in device */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_SEL, 0);
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_NUM, queue_size);
    
    /* Write descriptor ring address (split 64-bit address into low/high) */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(vq->desc_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(vq->desc_phys >> 32));
    
    /* Write available ring address */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(vq->avail_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(vq->avail_phys >> 32));
    
    /* Write used ring address */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(vq->used_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(vq->used_phys >> 32));
    
    /* Mark queue as ready */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    
    return 0;
}

/**
 * Allocate a chain of descriptors from the free list
 */
static int virtqueue_alloc_desc_chain(virtqueue_t *vq, uint16_t *desc_idx, uint32_t count)
{
    if (vq->num_free < count) {
        return -1;
    }
    
    *desc_idx = vq->free_head;
    uint16_t current = vq->free_head;
    
    /* Advance free_head by 'count' descriptors */
    for (uint32_t i = 0; i < count; i++) {
        current = vq->desc[current].next;
    }
    vq->free_head = current;
    vq->num_free -= count;
    
    return 0;
}

/**
 * Free a descriptor chain back to the free list
 */
static void virtqueue_free_desc_chain(virtqueue_t *vq, uint16_t desc_idx)
{
    /* Count descriptors in chain */
    uint16_t count = 1;
    uint16_t current = desc_idx;
    while (vq->desc[current].flags & VIRTQ_DESC_F_NEXT) {
        current = vq->desc[current].next;
        count++;
    }
    
    /* Add chain back to free list */
    vq->desc[current].next = vq->free_head;
    vq->free_head = desc_idx;
    vq->num_free += count;
}

/**
 * Add descriptor to available ring
 */
static void virtqueue_add_to_avail(virtqueue_t *vq, uint16_t desc_idx)
{
    uint16_t avail_idx = vq->avail->idx % vq->queue_size;
    vq->avail->ring[avail_idx] = desc_idx;
    
    /* Memory barrier to ensure descriptor writes complete before index update */
    write_barrier();
    
    vq->avail->idx++;
}

/**
 * Get buffer from used ring
 */
static int virtqueue_get_used_buf(virtqueue_t *vq, uint16_t *desc_idx, uint32_t *len)
{
    /* Memory barrier to ensure we read latest used ring index */
    read_barrier();
    
    if (vq->last_seen_used == vq->used->idx) {
        return -1;  // No new completions
    }
    
    uint16_t used_idx = vq->last_seen_used % vq->queue_size;
    *desc_idx = vq->used->ring[used_idx].id;
    *len = vq->used->ring[used_idx].len;
    
    vq->last_seen_used++;
    return 0;
}

/**
 * Notify device of new available buffers
 */
static void virtqueue_notify(virtio_blk_device_t *dev, uint32_t queue_idx)
{
    /* Memory barrier to ensure all writes complete before notify */
    write_barrier();
    
    /* Write queue index to QUEUE_NOTIFY register */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx);
    
    /* Memory barrier after notify */
    write_barrier();
}

/**
 * Perform a synchronous block I/O request
 */
static int virtio_blk_do_request(virtio_blk_device_t *dev, virtio_blk_request_t *req,
                                  uint64_t sector, void *buffer, uint32_t sectors,
                                  uint32_t type)
{
    virtqueue_t *vq = &dev->queue;
    
    /* Allocate 3 descriptors: header, data buffer, status */
    uint16_t desc_idx;
    if (virtqueue_alloc_desc_chain(vq, &desc_idx, 3) < 0) {
        return -1;
    }
    
    /* Setup request header */
    req->header.type = type;
    req->header.reserved = 0;
    req->header.sector = sector;
    req->data = buffer;
    req->status = 0xFF;
    
    /* Get physical addresses for all buffers */
    uintptr_t header_phys = translate_virt_to_phys((uintptr_t)&req->header);
    uintptr_t data_phys = translate_virt_to_phys((uintptr_t)buffer);
    uintptr_t status_phys = translate_virt_to_phys((uintptr_t)&req->status);
    
    if (header_phys == 0 || data_phys == 0 || status_phys == 0) {
        virtqueue_free_desc_chain(vq, desc_idx);
        return -1;
    }
    
    /* Descriptor 0: Request header (device reads) */
    uint16_t idx0 = desc_idx;
    uint16_t idx1 = vq->desc[idx0].next;  // Get pre-allocated next descriptor
    uint16_t idx2 = vq->desc[idx1].next;  // Get third descriptor
    
    vq->desc[idx0].addr = header_phys;
    vq->desc[idx0].len = sizeof(virtio_blk_req_header_t);
    vq->desc[idx0].flags = VIRTQ_DESC_F_NEXT;
    // idx0.next is already set to idx1 from allocation
    
    /* Descriptor 1: Data buffer (device reads for write, writes for read) */
    vq->desc[idx1].addr = data_phys;
    vq->desc[idx1].len = sectors * VIRTIO_BLK_SECTOR_SIZE;
    vq->desc[idx1].flags = VIRTQ_DESC_F_NEXT;
    if (type == VIRTIO_BLK_T_IN) {
        vq->desc[idx1].flags |= VIRTQ_DESC_F_WRITE;
    }
    // idx1.next is already set to idx2 from allocation
    
    /* Descriptor 2: Status byte (device writes) */
    vq->desc[idx2].addr = status_phys;
    vq->desc[idx2].len = 1;
    vq->desc[idx2].flags = VIRTQ_DESC_F_WRITE;  // Last descriptor, no NEXT flag
    vq->desc[idx2].next = 0;
    
    /* Memory barrier - ensure descriptor writes complete */
    write_barrier();
    
    /* Add to available ring and notify device */
    virtqueue_add_to_avail(vq, desc_idx);
    virtqueue_notify(dev, 0);
    
    /* Poll for completion (synchronous for now) */
    uint32_t timeout = 1000000;
    
    while (timeout > 0) {
        /* Check and acknowledge interrupt status even in polling mode */
        uint32_t int_status = VIRTIO_READ32(dev, VIRTIO_MMIO_INTERRUPT_STATUS);
        if (int_status) {
            VIRTIO_WRITE32(dev, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
        }
        
        uint16_t used_idx;
        uint32_t len;
        if (virtqueue_get_used_buf(vq, &used_idx, &len) == 0) {
            virtqueue_free_desc_chain(vq, used_idx);
            
            if (req->status != VIRTIO_BLK_S_OK) {
                return -1;
            }
            
            return sectors;
        }
        timeout--;
    }
    
    hal_uart_puts("[VirtIO] ERROR: Timeout\n");
    virtqueue_free_desc_chain(vq, desc_idx);
    return -1;
}

/**
 * Initialize VirtIO block device
 */
int virtio_blk_init(uintptr_t base_addr, uint32_t irq)
{
    /* Allocate device structure */
    g_blk_device = (virtio_blk_device_t *)kmalloc(sizeof(virtio_blk_device_t));
    if (!g_blk_device) {
        return -1;
    }
    
    g_blk_device->base_addr = base_addr;
    g_blk_device->irq = irq;
    g_blk_device->read_count = 0;
    g_blk_device->write_count = 0;
    g_blk_device->error_count = 0;
    
    /* Check magic value */
    uint32_t magic = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MAGIC) {
        kfree(g_blk_device);
        g_blk_device = NULL;
        return -1;
    }
    
    /* Check device version and ID */
    g_blk_device->version = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_VERSION);
    g_blk_device->device_id = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_DEVICE_ID);
    g_blk_device->vendor_id = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_VENDOR_ID);
    
    if (g_blk_device->device_id != VIRTIO_DEVICE_ID_BLOCK) {
        kfree(g_blk_device);
        g_blk_device = NULL;
        return -1;
    }
    
    hal_uart_puts("[VirtIO] Found block device\n");
    
    /* Reset device */
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_STATUS, 0);
    
    /* Device initialization sequence per VirtIO spec */
    uint32_t status = VIRTIO_STATUS_ACKNOWLEDGE;
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_STATUS, status);
    
    status |= VIRTIO_STATUS_DRIVER;
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_STATUS, status);
    
    /* Read device features */
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t features_low = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_DEVICE_FEATURES);
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t features_high = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_DEVICE_FEATURES);
    g_blk_device->features = ((uint64_t)features_high << 32) | features_low;
    
    /* Negotiate features (accept all for now) */
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DRIVER_FEATURES, features_low);
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_DRIVER_FEATURES, features_high);
    
    status |= VIRTIO_STATUS_FEATURES_OK;
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_STATUS, status);
    
    /* Verify features accepted */
    status = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        kfree(g_blk_device);
        g_blk_device = NULL;
        return -1;
    }
    
    /* Read device configuration */
    virtio_blk_config_t *config = (virtio_blk_config_t *)(g_blk_device->base_addr + VIRTIO_MMIO_CONFIG);
    g_blk_device->capacity = config->capacity;
    g_blk_device->block_size = (config->blk_size > 0) ? config->blk_size : VIRTIO_BLK_SECTOR_SIZE;
    g_blk_device->read_only = (g_blk_device->features & VIRTIO_BLK_F_RO) ? 1 : 0;
    
    /* Get maximum queue size */
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_QUEUE_SEL, 0);
    uint32_t queue_max = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    uint32_t queue_size = (queue_max < VIRTIO_BLK_QUEUE_SIZE) ? queue_max : VIRTIO_BLK_QUEUE_SIZE;
    
    /* Initialize virtqueue */
    if (virtqueue_init(g_blk_device, queue_size) < 0) {
        kfree(g_blk_device);
        g_blk_device = NULL;
        return -1;
    }
    
    /* Set DRIVER_OK status bit */
    status |= VIRTIO_STATUS_DRIVER_OK;
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_STATUS, status);
    
    /* Verify device accepted DRIVER_OK */
    uint32_t final_status = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_STATUS);
    if (!(final_status & VIRTIO_STATUS_DRIVER_OK)) {
        kfree(g_blk_device);
        g_blk_device = NULL;
        return -1;
    }
    
    return 0;
}

/**
 * Read sectors from block device
 */
int virtio_blk_read(uint64_t sector, void *buffer, uint32_t count)
{
    if (!g_blk_device) {
        return -1;
    }
    
    if (sector + count > g_blk_device->capacity) {
        return -1;
    }
    
    /* Allocate request structure from DMA memory (device needs to write status) */
    dma_region_t *req_region = dma_alloc(sizeof(virtio_blk_request_t), DMA_ZERO);
    if (!req_region) {
        return -1;
    }
    
    virtio_blk_request_t *req = (virtio_blk_request_t *)req_region->virt_addr;
    int result = virtio_blk_do_request(g_blk_device, req, sector, buffer, count, VIRTIO_BLK_T_IN);
    
    dma_free(req_region);
    
    if (result > 0) {
        g_blk_device->read_count++;
    } else {
        g_blk_device->error_count++;
    }
    
    return result;
}

/**
 * Write sectors to block device
 */
int virtio_blk_write(uint64_t sector, const void *buffer, uint32_t count)
{
    if (!g_blk_device) {
        return -1;
    }
    
    if (g_blk_device->read_only) {
        return -1;
    }
    
    if (sector + count > g_blk_device->capacity) {
        return -1;
    }
    
    /* Allocate request structure from DMA memory */
    dma_region_t *req_region = dma_alloc(sizeof(virtio_blk_request_t), DMA_ZERO);
    if (!req_region) {
        return -1;
    }
    
    virtio_blk_request_t *req = (virtio_blk_request_t *)req_region->virt_addr;
    int result = virtio_blk_do_request(g_blk_device, req, sector, (void *)buffer, count, VIRTIO_BLK_T_OUT);
    
    dma_free(req_region);
    
    if (result > 0) {
        g_blk_device->write_count++;
    } else {
        g_blk_device->error_count++;
    }
    
    return result;
}

/**
 * Flush device write cache
 */
int virtio_blk_flush(void)
{
    if (!g_blk_device) {
        return -1;
    }
    
    if (!(g_blk_device->features & VIRTIO_BLK_F_FLUSH)) {
        return 0;
    }
    
    virtio_blk_request_t req;
    return virtio_blk_do_request(g_blk_device, &req, 0, NULL, 0, VIRTIO_BLK_T_FLUSH);
}

/**
 * Get device capacity in sectors
 */
uint64_t virtio_blk_get_capacity(void)
{
    return g_blk_device ? g_blk_device->capacity : 0;
}

/**
 * Get device block size
 */
uint32_t virtio_blk_get_block_size(void)
{
    return g_blk_device ? g_blk_device->block_size : 0;
}

/**
 * Check if device is read-only
 */
int virtio_blk_is_readonly(void)
{
    return g_blk_device ? g_blk_device->read_only : 1;
}

/**
 * VirtIO block device interrupt handler
 */
void virtio_blk_irq_handler(void)
{
    if (!g_blk_device) {
        return;
    }
    
    /* Read and acknowledge interrupt */
    uint32_t int_status = VIRTIO_READ32(g_blk_device, VIRTIO_MMIO_INTERRUPT_STATUS);
    VIRTIO_WRITE32(g_blk_device, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
    
    /* TODO: Process used buffers asynchronously */
}

/**
 * Get the global VirtIO block device
 */
virtio_blk_device_t *virtio_blk_get_device(void)
{
    return g_blk_device;
}

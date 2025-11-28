/*
 * VirtIO GPU Device Driver
 * 
 * Implementation of VirtIO 1.0+ GPU device driver using MMIO interface.
 * Provides 2D framebuffer rendering with scanout support.
 */

#include <drivers/virtio_gpu.h>
#include <drivers/virtio_blk.h>  /* For MMIO register offsets */
#include <mm/dma.h>
#include <mm/paging.h>
#include <mm/kmalloc.h>
#include <arch/barrier.h>
#include <hal/hal_uart.h>
#include <kernel/errno.h>
#include <stddef.h>

/* Helper macros for MMIO register access */
#define GPU_READ32(dev, offset) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))))

#define GPU_WRITE32(dev, offset, value) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))) = (value))

/* Global device state */
static virtio_gpu_device_t *g_gpu_device = NULL;

/* DMA regions for commands/responses */
static dma_region_t *g_cmd_region = NULL;
static dma_region_t *g_resp_region = NULL;

/* Forward declarations */
static int gpu_queue_init(virtio_gpu_device_t *dev, virtio_gpu_queue_t *vq, 
                          uint32_t queue_idx, uint32_t queue_size);
static int gpu_send_command(void *cmd, size_t cmd_size, void *resp, size_t resp_size);
static int gpu_get_display_info_internal(void);
static int gpu_create_resource(uint32_t resource_id, uint32_t format, 
                               uint32_t width, uint32_t height);
static int gpu_attach_backing(uint32_t resource_id, uintptr_t phys_addr, size_t size);
static int gpu_set_scanout(uint32_t scanout_id, uint32_t resource_id,
                           uint32_t width, uint32_t height);
static int gpu_transfer_to_host(uint32_t resource_id, uint32_t x, uint32_t y,
                                uint32_t width, uint32_t height);
static int gpu_resource_flush(uint32_t resource_id, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height);

/**
 * Initialize a virtqueue for GPU
 */
static int gpu_queue_init(virtio_gpu_device_t *dev, virtio_gpu_queue_t *vq,
                          uint32_t queue_idx, uint32_t queue_size)
{
    vq->queue_size = queue_size;
    vq->last_seen_used = 0;
    vq->num_free = queue_size;
    
    /* Calculate sizes for each ring */
    size_t desc_size = sizeof(virtio_gpu_desc_t) * queue_size;
    size_t avail_size = sizeof(uint16_t) * (3 + queue_size);
    size_t used_size = sizeof(uint16_t) * 3 + sizeof(virtio_gpu_used_elem_t) * queue_size;
    
    /* Allocate descriptor ring */
    dma_region_t *desc_region = dma_alloc(desc_size, DMA_ZERO);
    if (!desc_region) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->desc = (virtio_gpu_desc_t *)desc_region->virt_addr;
    vq->desc_phys = desc_region->phys_addr;
    
    /* Allocate available ring */
    dma_region_t *avail_region = dma_alloc(avail_size, DMA_ZERO);
    if (!avail_region) {
        dma_free(desc_region);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->avail = (virtio_gpu_avail_t *)avail_region->virt_addr;
    vq->avail_phys = avail_region->phys_addr;
    
    /* Allocate used ring */
    dma_region_t *used_region = dma_alloc(used_size, DMA_ZERO);
    if (!used_region) {
        dma_free(desc_region);
        dma_free(avail_region);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->used = (virtio_gpu_used_t *)used_region->virt_addr;
    vq->used_phys = used_region->phys_addr;
    
    /* Initialize free descriptor list */
    for (uint16_t i = 0; i < queue_size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[queue_size - 1].next = 0;
    vq->free_head = 0;
    
    /* Configure queue in device */
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_SEL, queue_idx);
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_NUM, queue_size);
    
    /* Write descriptor ring address */
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(vq->desc_phys & 0xFFFFFFFF));
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(vq->desc_phys >> 32));
    
    /* Write available ring address */
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(vq->avail_phys & 0xFFFFFFFF));
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(vq->avail_phys >> 32));
    
    /* Write used ring address */
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(vq->used_phys & 0xFFFFFFFF));
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(vq->used_phys >> 32));
    
    /* Mark queue as ready */
    GPU_WRITE32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    
    clear_errno();
    return 0;
}

/**
 * Send a command to the GPU and wait for response
 */
static int gpu_send_command(void *cmd, size_t cmd_size, void *resp, size_t resp_size)
{
    if (!g_gpu_device) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    virtio_gpu_queue_t *vq = &g_gpu_device->controlq;
    
    /* Check we have enough free descriptors */
    if (vq->num_free < 2) {
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    /* Get physical addresses */
    uintptr_t cmd_phys = translate_virt_to_phys((uintptr_t)cmd);
    uintptr_t resp_phys = translate_virt_to_phys((uintptr_t)resp);
    
    if (cmd_phys == 0 || resp_phys == 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Allocate two descriptors */
    uint16_t desc0 = vq->free_head;
    uint16_t desc1 = vq->desc[desc0].next;
    vq->free_head = vq->desc[desc1].next;
    vq->num_free -= 2;
    
    /* Setup command descriptor (device reads) */
    vq->desc[desc0].addr = cmd_phys;
    vq->desc[desc0].len = cmd_size;
    vq->desc[desc0].flags = 1;  /* VIRTQ_DESC_F_NEXT */
    vq->desc[desc0].next = desc1;
    
    /* Setup response descriptor (device writes) */
    vq->desc[desc1].addr = resp_phys;
    vq->desc[desc1].len = resp_size;
    vq->desc[desc1].flags = 2;  /* VIRTQ_DESC_F_WRITE */
    vq->desc[desc1].next = 0;
    
    /* Memory barrier */
    write_barrier();
    
    /* Add to available ring */
    uint16_t avail_idx = vq->avail->idx % vq->queue_size;
    vq->avail->ring[avail_idx] = desc0;
    write_barrier();
    vq->avail->idx++;
    
    /* Notify device */
    write_barrier();
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_QUEUE_NOTIFY, VIRTIO_GPU_QUEUE_CONTROL);
    write_barrier();
    
    /* Poll for completion */
    uint32_t timeout = 1000000;
    while (timeout > 0) {
        /* Check interrupt status */
        uint32_t int_status = GPU_READ32(g_gpu_device, VIRTIO_MMIO_INTERRUPT_STATUS);
        if (int_status) {
            GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
        }
        
        read_barrier();
        if (vq->last_seen_used != vq->used->idx) {
            /* Got completion */
            vq->last_seen_used++;
            
            /* Return descriptors to free list */
            vq->desc[desc1].next = vq->free_head;
            vq->desc[desc0].next = desc1;
            vq->free_head = desc0;
            vq->num_free += 2;
            
            /* Check response type */
            virtio_gpu_ctrl_hdr_t *hdr = (virtio_gpu_ctrl_hdr_t *)resp;
            if (hdr->type >= VIRTIO_GPU_RESP_ERR_UNSPEC) {
                g_gpu_device->error_count++;
                RETURN_ERRNO(THUNDEROS_EIO);
            }
            
            clear_errno();
            return 0;
        }
        timeout--;
    }
    
    /* Return descriptors on timeout */
    vq->desc[desc1].next = vq->free_head;
    vq->desc[desc0].next = desc1;
    vq->free_head = desc0;
    vq->num_free += 2;
    
    RETURN_ERRNO(THUNDEROS_EVIRTIO_TIMEOUT);
}

/**
 * Get display information from GPU
 */
static int gpu_get_display_info_internal(void)
{
    /* Prepare command */
    virtio_gpu_ctrl_hdr_t *cmd = (virtio_gpu_ctrl_hdr_t *)g_cmd_region->virt_addr;
    cmd->type = VIRTIO_GPU_CMD_GET_DISPLAY_INFO;
    cmd->flags = 0;
    cmd->fence_id = 0;
    cmd->ctx_id = 0;
    cmd->ring_idx = 0;
    
    /* Prepare response buffer */
    virtio_gpu_resp_display_info_t *resp = 
        (virtio_gpu_resp_display_info_t *)g_resp_region->virt_addr;
    
    /* Send command */
    if (gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp)) < 0) {
        return -1;
    }
    
    /* Copy display info manually (no memcpy in freestanding) */
    g_gpu_device->num_scanouts = 0;
    for (int i = 0; i < VIRTIO_GPU_MAX_SCANOUTS; i++) {
        g_gpu_device->displays[i].r.x = resp->pmodes[i].r.x;
        g_gpu_device->displays[i].r.y = resp->pmodes[i].r.y;
        g_gpu_device->displays[i].r.width = resp->pmodes[i].r.width;
        g_gpu_device->displays[i].r.height = resp->pmodes[i].r.height;
        g_gpu_device->displays[i].enabled = resp->pmodes[i].enabled;
        g_gpu_device->displays[i].flags = resp->pmodes[i].flags;
        if (resp->pmodes[i].enabled) {
            g_gpu_device->num_scanouts++;
        }
    }
    
    clear_errno();
    return 0;
}

/**
 * Create a 2D resource
 */
static int gpu_create_resource(uint32_t resource_id, uint32_t format,
                               uint32_t width, uint32_t height)
{
    virtio_gpu_resource_create_2d_t *cmd = 
        (virtio_gpu_resource_create_2d_t *)g_cmd_region->virt_addr;
    
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_CREATE_2D;
    cmd->hdr.flags = 0;
    cmd->hdr.fence_id = 0;
    cmd->hdr.ctx_id = 0;
    cmd->hdr.ring_idx = 0;
    cmd->resource_id = resource_id;
    cmd->format = format;
    cmd->width = width;
    cmd->height = height;
    
    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)g_resp_region->virt_addr;
    
    return gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp));
}

/**
 * Attach backing storage to a resource
 */
static int gpu_attach_backing(uint32_t resource_id, uintptr_t phys_addr, size_t size)
{
    /* Need space for command header + one memory entry */
    struct {
        virtio_gpu_resource_attach_backing_t hdr;
        virtio_gpu_mem_entry_t entry;
    } __attribute__((packed)) *cmd = 
        (void *)g_cmd_region->virt_addr;
    
    cmd->hdr.hdr.type = VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING;
    cmd->hdr.hdr.flags = 0;
    cmd->hdr.hdr.fence_id = 0;
    cmd->hdr.hdr.ctx_id = 0;
    cmd->hdr.hdr.ring_idx = 0;
    cmd->hdr.resource_id = resource_id;
    cmd->hdr.nr_entries = 1;
    
    cmd->entry.addr = phys_addr;
    cmd->entry.length = size;
    cmd->entry.padding = 0;
    
    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)g_resp_region->virt_addr;
    
    return gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp));
}

/**
 * Set scanout (connect resource to display)
 */
static int gpu_set_scanout(uint32_t scanout_id, uint32_t resource_id,
                           uint32_t width, uint32_t height)
{
    virtio_gpu_set_scanout_t *cmd = 
        (virtio_gpu_set_scanout_t *)g_cmd_region->virt_addr;
    
    cmd->hdr.type = VIRTIO_GPU_CMD_SET_SCANOUT;
    cmd->hdr.flags = 0;
    cmd->hdr.fence_id = 0;
    cmd->hdr.ctx_id = 0;
    cmd->hdr.ring_idx = 0;
    cmd->r.x = 0;
    cmd->r.y = 0;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->scanout_id = scanout_id;
    cmd->resource_id = resource_id;
    
    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)g_resp_region->virt_addr;
    
    return gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp));
}

/**
 * Transfer framebuffer data to host
 */
static int gpu_transfer_to_host(uint32_t resource_id, uint32_t x, uint32_t y,
                                uint32_t width, uint32_t height)
{
    virtio_gpu_transfer_to_host_2d_t *cmd = 
        (virtio_gpu_transfer_to_host_2d_t *)g_cmd_region->virt_addr;
    
    cmd->hdr.type = VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D;
    cmd->hdr.flags = 0;
    cmd->hdr.fence_id = 0;
    cmd->hdr.ctx_id = 0;
    cmd->hdr.ring_idx = 0;
    cmd->r.x = x;
    cmd->r.y = y;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->offset = 0;
    cmd->resource_id = resource_id;
    cmd->padding = 0;
    
    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)g_resp_region->virt_addr;
    
    return gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp));
}

/**
 * Flush resource to display
 */
static int gpu_resource_flush(uint32_t resource_id, uint32_t x, uint32_t y,
                              uint32_t width, uint32_t height)
{
    virtio_gpu_resource_flush_t *cmd = 
        (virtio_gpu_resource_flush_t *)g_cmd_region->virt_addr;
    
    cmd->hdr.type = VIRTIO_GPU_CMD_RESOURCE_FLUSH;
    cmd->hdr.flags = 0;
    cmd->hdr.fence_id = 0;
    cmd->hdr.ctx_id = 0;
    cmd->hdr.ring_idx = 0;
    cmd->r.x = x;
    cmd->r.y = y;
    cmd->r.width = width;
    cmd->r.height = height;
    cmd->resource_id = resource_id;
    cmd->padding = 0;
    
    virtio_gpu_ctrl_hdr_t *resp = (virtio_gpu_ctrl_hdr_t *)g_resp_region->virt_addr;
    
    return gpu_send_command(cmd, sizeof(*cmd), resp, sizeof(*resp));
}

/* ============================================================================
 * Public API Implementation
 * ============================================================================ */

/**
 * Initialize VirtIO GPU device
 */
int virtio_gpu_init(uintptr_t base_addr, uint32_t irq)
{
    /* Allocate device structure */
    g_gpu_device = (virtio_gpu_device_t *)kmalloc(sizeof(virtio_gpu_device_t));
    if (!g_gpu_device) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Zero out the structure */
    uint8_t *ptr = (uint8_t *)g_gpu_device;
    for (size_t i = 0; i < sizeof(virtio_gpu_device_t); i++) {
        ptr[i] = 0;
    }
    
    g_gpu_device->base_addr = base_addr;
    g_gpu_device->irq = irq;
    
    /* Check magic value */
    uint32_t magic = GPU_READ32(g_gpu_device, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MAGIC) {
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Check device version and ID */
    g_gpu_device->version = GPU_READ32(g_gpu_device, VIRTIO_MMIO_VERSION);
    g_gpu_device->device_id = GPU_READ32(g_gpu_device, VIRTIO_MMIO_DEVICE_ID);
    g_gpu_device->vendor_id = GPU_READ32(g_gpu_device, VIRTIO_MMIO_VENDOR_ID);
    
    if (g_gpu_device->device_id != VIRTIO_DEVICE_ID_GPU) {
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Reset device */
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, 0);
    
    /* Device initialization sequence */
    uint32_t status = VIRTIO_STATUS_ACKNOWLEDGE;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, status);
    
    status |= VIRTIO_STATUS_DRIVER;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, status);
    
    /* Read device features */
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t features_low = GPU_READ32(g_gpu_device, VIRTIO_MMIO_DEVICE_FEATURES);
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t features_high = GPU_READ32(g_gpu_device, VIRTIO_MMIO_DEVICE_FEATURES);
    g_gpu_device->features = ((uint64_t)features_high << 32) | features_low;
    
    /* Accept basic features (no 3D/VIRGL) */
    uint32_t accepted_low = features_low & ~(uint32_t)VIRTIO_GPU_F_VIRGL;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DRIVER_FEATURES, accepted_low);
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_DRIVER_FEATURES, features_high);
    
    /* Features OK */
    status |= VIRTIO_STATUS_FEATURES_OK;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, status);
    
    /* Verify features OK */
    uint32_t status_check = GPU_READ32(g_gpu_device, VIRTIO_MMIO_STATUS);
    if (!(status_check & VIRTIO_STATUS_FEATURES_OK)) {
        GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Initialize control queue */
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_QUEUE_SEL, VIRTIO_GPU_QUEUE_CONTROL);
    uint32_t max_queue_size = GPU_READ32(g_gpu_device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    uint32_t queue_size = (max_queue_size < VIRTIO_GPU_QUEUE_SIZE) ? 
                          max_queue_size : VIRTIO_GPU_QUEUE_SIZE;
    
    if (gpu_queue_init(g_gpu_device, &g_gpu_device->controlq, 
                       VIRTIO_GPU_QUEUE_CONTROL, queue_size) < 0) {
        GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        return -1;  /* errno already set */
    }
    
    /* Driver OK */
    status |= VIRTIO_STATUS_DRIVER_OK;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, status);
    
    /* Allocate DMA regions for commands and responses */
    g_cmd_region = dma_alloc(4096, DMA_ZERO);
    g_resp_region = dma_alloc(4096, DMA_ZERO);
    if (!g_cmd_region || !g_resp_region) {
        if (g_cmd_region) dma_free(g_cmd_region);
        if (g_resp_region) dma_free(g_resp_region);
        GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Get display info */
    if (gpu_get_display_info_internal() < 0) {
        hal_uart_puts("[GPU] Warning: Failed to get display info\n");
    }
    
    /* Use default or detected dimensions */
    if (g_gpu_device->num_scanouts > 0 && g_gpu_device->displays[0].enabled) {
        g_gpu_device->fb_width = g_gpu_device->displays[0].r.width;
        g_gpu_device->fb_height = g_gpu_device->displays[0].r.height;
    } else {
        g_gpu_device->fb_width = VIRTIO_GPU_DEFAULT_WIDTH;
        g_gpu_device->fb_height = VIRTIO_GPU_DEFAULT_HEIGHT;
    }
    
    g_gpu_device->fb_format = VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM;
    g_gpu_device->fb_size = g_gpu_device->fb_width * g_gpu_device->fb_height * 4;
    
    /* Allocate framebuffer */
    dma_region_t *fb_region = dma_alloc(g_gpu_device->fb_size, DMA_ZERO);
    if (!fb_region) {
        dma_free(g_cmd_region);
        dma_free(g_resp_region);
        GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
        kfree(g_gpu_device);
        g_gpu_device = NULL;
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    g_gpu_device->fb_pixels = (uint32_t *)fb_region->virt_addr;
    g_gpu_device->fb_phys = fb_region->phys_addr;
    g_gpu_device->resource_id = 1;
    
    /* Create GPU resource */
    if (gpu_create_resource(g_gpu_device->resource_id, g_gpu_device->fb_format,
                            g_gpu_device->fb_width, g_gpu_device->fb_height) < 0) {
        hal_uart_puts("[GPU] Failed to create resource\n");
        goto fail;
    }
    
    /* Attach backing storage */
    if (gpu_attach_backing(g_gpu_device->resource_id, g_gpu_device->fb_phys,
                           g_gpu_device->fb_size) < 0) {
        hal_uart_puts("[GPU] Failed to attach backing\n");
        goto fail;
    }
    
    /* Set scanout */
    if (gpu_set_scanout(0, g_gpu_device->resource_id,
                        g_gpu_device->fb_width, g_gpu_device->fb_height) < 0) {
        hal_uart_puts("[GPU] Failed to set scanout\n");
        goto fail;
    }
    
    hal_uart_puts("[GPU] VirtIO GPU initialized: ");
    /* Print dimensions - simple decimal printing */
    char buf[16];
    int n = 0;
    uint32_t w = g_gpu_device->fb_width;
    uint32_t h = g_gpu_device->fb_height;
    
    /* Width */
    if (w == 0) {
        hal_uart_putc('0');
    } else {
        n = 0;
        while (w > 0) { buf[n++] = '0' + (w % 10); w /= 10; }
        while (n > 0) { hal_uart_putc(buf[--n]); }
    }
    hal_uart_putc('x');
    
    /* Height */
    if (h == 0) {
        hal_uart_putc('0');
    } else {
        n = 0;
        while (h > 0) { buf[n++] = '0' + (h % 10); h /= 10; }
        while (n > 0) { hal_uart_putc(buf[--n]); }
    }
    hal_uart_puts("\n");
    
    clear_errno();
    return 0;
    
fail:
    dma_free(fb_region);
    dma_free(g_cmd_region);
    dma_free(g_resp_region);
    g_cmd_region = NULL;
    g_resp_region = NULL;
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, VIRTIO_STATUS_FAILED);
    kfree(g_gpu_device);
    g_gpu_device = NULL;
    RETURN_ERRNO(THUNDEROS_EIO);
}

/**
 * Check if VirtIO GPU is available
 */
int virtio_gpu_available(void)
{
    return g_gpu_device != NULL;
}

/**
 * Get display information
 */
int virtio_gpu_get_display_info(uint32_t *width, uint32_t *height)
{
    if (!g_gpu_device) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    if (width) *width = g_gpu_device->fb_width;
    if (height) *height = g_gpu_device->fb_height;
    
    clear_errno();
    return 0;
}

/**
 * Set a pixel in the framebuffer
 */
void virtio_gpu_set_pixel(uint32_t x, uint32_t y, uint32_t color)
{
    if (!g_gpu_device || !g_gpu_device->fb_pixels) return;
    if (x >= g_gpu_device->fb_width || y >= g_gpu_device->fb_height) return;
    
    /* Convert ARGB to BGRX for the GPU format */
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint32_t bgrx = (r << 16) | (g << 8) | b;
    
    g_gpu_device->fb_pixels[y * g_gpu_device->fb_width + x] = bgrx;
}

/**
 * Get a pixel from the framebuffer
 */
uint32_t virtio_gpu_get_pixel(uint32_t x, uint32_t y)
{
    if (!g_gpu_device || !g_gpu_device->fb_pixels) return 0;
    if (x >= g_gpu_device->fb_width || y >= g_gpu_device->fb_height) return 0;
    
    uint32_t bgrx = g_gpu_device->fb_pixels[y * g_gpu_device->fb_width + x];
    /* Convert BGRX back to ARGB */
    uint8_t r = (bgrx >> 16) & 0xFF;
    uint8_t g = (bgrx >> 8) & 0xFF;
    uint8_t b = bgrx & 0xFF;
    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/**
 * Clear the framebuffer
 */
void virtio_gpu_clear(uint32_t color)
{
    if (!g_gpu_device || !g_gpu_device->fb_pixels) return;
    
    /* Convert ARGB to BGRX */
    uint8_t r = (color >> 16) & 0xFF;
    uint8_t g = (color >> 8) & 0xFF;
    uint8_t b = color & 0xFF;
    uint32_t bgrx = (r << 16) | (g << 8) | b;
    
    uint32_t num_pixels = g_gpu_device->fb_width * g_gpu_device->fb_height;
    for (uint32_t i = 0; i < num_pixels; i++) {
        g_gpu_device->fb_pixels[i] = bgrx;
    }
}

/**
 * Flush framebuffer to display
 */
int virtio_gpu_flush(void)
{
    if (!g_gpu_device) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    /* Transfer to host */
    if (gpu_transfer_to_host(g_gpu_device->resource_id, 0, 0,
                             g_gpu_device->fb_width, g_gpu_device->fb_height) < 0) {
        return -1;
    }
    
    /* Flush to display */
    if (gpu_resource_flush(g_gpu_device->resource_id, 0, 0,
                           g_gpu_device->fb_width, g_gpu_device->fb_height) < 0) {
        return -1;
    }
    
    g_gpu_device->flush_count++;
    clear_errno();
    return 0;
}

/**
 * Flush a region of the framebuffer
 */
int virtio_gpu_flush_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (!g_gpu_device) {
        RETURN_ERRNO(THUNDEROS_ENODEV);
    }
    
    /* Clamp to framebuffer bounds */
    if (x >= g_gpu_device->fb_width || y >= g_gpu_device->fb_height) {
        clear_errno();
        return 0;
    }
    if (x + width > g_gpu_device->fb_width) {
        width = g_gpu_device->fb_width - x;
    }
    if (y + height > g_gpu_device->fb_height) {
        height = g_gpu_device->fb_height - y;
    }
    
    /* Transfer to host */
    if (gpu_transfer_to_host(g_gpu_device->resource_id, x, y, width, height) < 0) {
        return -1;
    }
    
    /* Flush to display */
    if (gpu_resource_flush(g_gpu_device->resource_id, x, y, width, height) < 0) {
        return -1;
    }
    
    g_gpu_device->flush_count++;
    clear_errno();
    return 0;
}

/**
 * Get framebuffer pointer
 */
uint32_t *virtio_gpu_get_framebuffer(void)
{
    if (!g_gpu_device) return NULL;
    return g_gpu_device->fb_pixels;
}

/**
 * Get framebuffer dimensions
 */
void virtio_gpu_get_dimensions(uint32_t *width, uint32_t *height)
{
    if (!g_gpu_device) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = g_gpu_device->fb_width;
    if (height) *height = g_gpu_device->fb_height;
}

/**
 * Shutdown VirtIO GPU device
 */
void virtio_gpu_shutdown(void)
{
    if (!g_gpu_device) return;
    
    /* Reset device */
    GPU_WRITE32(g_gpu_device, VIRTIO_MMIO_STATUS, 0);
    
    /* Free resources */
    if (g_cmd_region) {
        dma_free(g_cmd_region);
        g_cmd_region = NULL;
    }
    if (g_resp_region) {
        dma_free(g_resp_region);
        g_resp_region = NULL;
    }
    
    kfree(g_gpu_device);
    g_gpu_device = NULL;
}

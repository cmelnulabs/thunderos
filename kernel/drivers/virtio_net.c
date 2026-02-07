/*
 * VirtIO Network Device Driver
 * 
 * Implementation of VirtIO 1.0+ network device driver using MMIO interface.
 * Provides packet send/receive with DMA-allocated buffers.
 */

#include <drivers/virtio_net.h>
#include <drivers/virtio_blk.h>  /* For shared MMIO register definitions */
#include <mm/dma.h>
#include <mm/paging.h>
#include <mm/kmalloc.h>
#include <arch/barrier.h>
#include <hal/hal_uart.h>
#include <kernel/errno.h>
#include <stddef.h>
#include <stdint.h>

/* Helper macros for MMIO register access */
#define VIRTIO_READ32(dev, offset) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))))

#define VIRTIO_WRITE32(dev, offset, value) \
    (*((volatile uint32_t *)((dev)->base_addr + (offset))) = (value))

/* VirtIO MMIO register offsets (shared with block device) */
#define VIRTIO_MMIO_MAGIC_VALUE         0x000
#define VIRTIO_MMIO_VERSION             0x004
#define VIRTIO_MMIO_DEVICE_ID           0x008
#define VIRTIO_MMIO_VENDOR_ID           0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES     0x010
#define VIRTIO_MMIO_DEVICE_FEATURES_SEL 0x014
#define VIRTIO_MMIO_DRIVER_FEATURES     0x020
#define VIRTIO_MMIO_DRIVER_FEATURES_SEL 0x024
#define VIRTIO_MMIO_QUEUE_SEL           0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX       0x034
#define VIRTIO_MMIO_QUEUE_NUM           0x038
#define VIRTIO_MMIO_QUEUE_READY         0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY        0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS    0x060
#define VIRTIO_MMIO_INTERRUPT_ACK       0x064
#define VIRTIO_MMIO_STATUS              0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW      0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH     0x084
#define VIRTIO_MMIO_QUEUE_AVAIL_LOW     0x090
#define VIRTIO_MMIO_QUEUE_AVAIL_HIGH    0x094
#define VIRTIO_MMIO_QUEUE_USED_LOW      0x0a0
#define VIRTIO_MMIO_QUEUE_USED_HIGH     0x0a4
#define VIRTIO_MMIO_CONFIG              0x100

/* VirtIO magic and status values */
#define VIRTIO_MAGIC                    0x74726976
#define VIRTIO_STATUS_ACKNOWLEDGE       (1 << 0)
#define VIRTIO_STATUS_DRIVER            (1 << 1)
#define VIRTIO_STATUS_DRIVER_OK         (1 << 2)
#define VIRTIO_STATUS_FEATURES_OK       (1 << 3)
#define VIRTIO_STATUS_FAILED            (1 << 7)

/* VirtIO descriptor flags */
#define VIRTQ_DESC_F_NEXT               1
#define VIRTQ_DESC_F_WRITE              2

/* Timeout for TX completion polling */
#define VIRTIO_NET_TX_TIMEOUT           1000000

/* Global device state */
static virtio_net_device_t *g_net_device = NULL;

/* Forward declarations */
static int virtqueue_net_init(virtio_net_device_t *dev, virtqueue_net_t *vq,
                               uint32_t queue_idx, uint32_t queue_size);
static int virtqueue_alloc_desc(virtqueue_net_t *vq, uint16_t *desc_idx);
static void virtqueue_free_desc(virtqueue_net_t *vq, uint16_t desc_idx);
static void virtqueue_add_to_avail(virtqueue_net_t *vq, uint16_t desc_idx);
static int virtqueue_get_used_buf(virtqueue_net_t *vq, uint16_t *desc_idx, uint32_t *len);
static void virtqueue_notify(virtio_net_device_t *dev, uint32_t queue_idx);
static int post_rx_buffers(virtio_net_device_t *dev);

/**
 * Initialize a virtqueue for network device
 */
static int virtqueue_net_init(virtio_net_device_t *dev, virtqueue_net_t *vq,
                               uint32_t queue_idx, uint32_t queue_size)
{
    vq->queue_size = queue_size;
    vq->last_seen_used = 0;
    vq->num_free = queue_size;
    
    /* Calculate sizes */
    size_t desc_size = sizeof(virtq_net_desc_t) * queue_size;
    size_t avail_size = sizeof(uint16_t) * (3 + queue_size);
    size_t used_size = sizeof(uint16_t) * 3 + sizeof(virtq_net_used_elem_t) * queue_size;
    
    /* Allocate descriptor ring */
    dma_region_t *desc_region = dma_alloc(desc_size, DMA_ZERO);
    if (!desc_region) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->desc = (virtq_net_desc_t *)desc_region->virt_addr;
    vq->desc_phys = desc_region->phys_addr;
    
    /* Allocate available ring */
    dma_region_t *avail_region = dma_alloc(avail_size, DMA_ZERO);
    if (!avail_region) {
        dma_free(desc_region);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->avail = (virtq_net_avail_t *)avail_region->virt_addr;
    vq->avail_phys = avail_region->phys_addr;
    
    /* Allocate used ring */
    dma_region_t *used_region = dma_alloc(used_size, DMA_ZERO);
    if (!used_region) {
        dma_free(desc_region);
        dma_free(avail_region);
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    vq->used = (virtq_net_used_t *)used_region->virt_addr;
    vq->used_phys = used_region->phys_addr;
    
    /* Initialize free descriptor list */
    for (uint16_t i = 0; i < queue_size - 1; i++) {
        vq->desc[i].next = i + 1;
    }
    vq->desc[queue_size - 1].next = 0;
    vq->free_head = 0;
    
    /* Configure queue in device */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_SEL, queue_idx);
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_NUM, queue_size);
    
    /* Write ring addresses */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_LOW, (uint32_t)(vq->desc_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_DESC_HIGH, (uint32_t)(vq->desc_phys >> 32));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_LOW, (uint32_t)(vq->avail_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_AVAIL_HIGH, (uint32_t)(vq->avail_phys >> 32));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_LOW, (uint32_t)(vq->used_phys & 0xFFFFFFFF));
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_USED_HIGH, (uint32_t)(vq->used_phys >> 32));
    
    /* Mark queue as ready */
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_READY, 1);
    
    clear_errno();
    return 0;
}

/**
 * Allocate a single descriptor from free list
 */
static int virtqueue_alloc_desc(virtqueue_net_t *vq, uint16_t *desc_idx)
{
    if (vq->num_free == 0) {
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    *desc_idx = vq->free_head;
    vq->free_head = vq->desc[vq->free_head].next;
    vq->num_free--;
    
    clear_errno();
    return 0;
}

/**
 * Free a descriptor back to free list
 */
static void virtqueue_free_desc(virtqueue_net_t *vq, uint16_t desc_idx)
{
    vq->desc[desc_idx].next = vq->free_head;
    vq->free_head = desc_idx;
    vq->num_free++;
}

/**
 * Add descriptor to available ring
 */
static void virtqueue_add_to_avail(virtqueue_net_t *vq, uint16_t desc_idx)
{
    uint16_t avail_idx = vq->avail->idx % vq->queue_size;
    vq->avail->ring[avail_idx] = desc_idx;
    write_barrier();
    vq->avail->idx++;
}

/**
 * Get buffer from used ring
 */
static int virtqueue_get_used_buf(virtqueue_net_t *vq, uint16_t *desc_idx, uint32_t *len)
{
    read_barrier();
    
    /* Read used index directly to avoid unaligned pointer warning */
    uint16_t device_idx = vq->used->idx;
    
    if (vq->last_seen_used == device_idx) {
        return -1;  /* No new completions */
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
static void virtqueue_notify(virtio_net_device_t *dev, uint32_t queue_idx)
{
    write_barrier();
    VIRTIO_WRITE32(dev, VIRTIO_MMIO_QUEUE_NOTIFY, queue_idx);
    write_barrier();
}

/**
 * Post RX buffers to receive queue
 */
static int post_rx_buffers(virtio_net_device_t *dev)
{
    virtqueue_net_t *vq = &dev->rx_queue;
    int posted = 0;
    
    for (int i = 0; i < VIRTIO_NET_RX_BUFFERS; i++) {
        virtio_net_rx_buf_t *rxbuf = &dev->rx_buffers[i];
        
        if (rxbuf->in_use) {
            continue;
        }
        
        /* Allocate buffer if not already allocated */
        if (!rxbuf->buffer) {
            size_t buf_size = sizeof(virtio_net_hdr_t) + VIRTIO_NET_MAX_PACKET_SIZE;
            dma_region_t *region = dma_alloc(buf_size, DMA_ZERO);
            if (!region) {
                continue;
            }
            rxbuf->buffer = (void *)region->virt_addr;
            rxbuf->buffer_phys = region->phys_addr;
        }
        
        /* Allocate descriptor */
        uint16_t desc_idx;
        if (virtqueue_alloc_desc(vq, &desc_idx) < 0) {
            break;
        }
        
        /* Setup descriptor for receive */
        vq->desc[desc_idx].addr = rxbuf->buffer_phys;
        vq->desc[desc_idx].len = sizeof(virtio_net_hdr_t) + VIRTIO_NET_MAX_PACKET_SIZE;
        vq->desc[desc_idx].flags = VIRTQ_DESC_F_WRITE;  /* Device writes to this buffer */
        vq->desc[desc_idx].next = 0;
        
        rxbuf->desc_idx = desc_idx;
        rxbuf->in_use = 1;
        
        virtqueue_add_to_avail(vq, desc_idx);
        posted++;
    }
    
    if (posted > 0) {
        virtqueue_notify(dev, VIRTIO_NET_QUEUE_RX);
    }
    
    return posted;
}

/**
 * Initialize VirtIO network device
 */
int virtio_net_init(uintptr_t base_addr, uint32_t irq)
{
    /* Check if already initialized */
    if (g_net_device) {
        clear_errno();
        return 0;
    }
    
    /* Allocate device structure */
    g_net_device = (virtio_net_device_t *)kmalloc(sizeof(virtio_net_device_t));
    if (!g_net_device) {
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Zero initialize */
    for (size_t i = 0; i < sizeof(virtio_net_device_t); i++) {
        ((uint8_t *)g_net_device)[i] = 0;
    }
    
    g_net_device->base_addr = base_addr;
    g_net_device->irq = irq;
    
    /* Check magic value */
    uint32_t magic = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_MAGIC_VALUE);
    if (magic != VIRTIO_MAGIC) {
        kfree(g_net_device);
        g_net_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Check device version and ID */
    g_net_device->version = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_VERSION);
    g_net_device->device_id = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_DEVICE_ID);
    g_net_device->vendor_id = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_VENDOR_ID);
    
    if (g_net_device->device_id != VIRTIO_DEVICE_ID_NET) {
        kfree(g_net_device);
        g_net_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Reset device */
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_STATUS, 0);
    
    /* Device initialization sequence */
    uint32_t status = VIRTIO_STATUS_ACKNOWLEDGE;
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_STATUS, status);
    
    status |= VIRTIO_STATUS_DRIVER;
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_STATUS, status);
    
    /* Read device features */
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 0);
    uint32_t features_low = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_DEVICE_FEATURES);
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DEVICE_FEATURES_SEL, 1);
    uint32_t features_high = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_DEVICE_FEATURES);
    uint64_t device_features = ((uint64_t)features_high << 32) | features_low;
    
    /* Accept only features we understand */
    uint64_t driver_features = 0;
    if (device_features & VIRTIO_NET_F_MAC) {
        driver_features |= VIRTIO_NET_F_MAC;
    }
    if (device_features & VIRTIO_NET_F_STATUS) {
        driver_features |= VIRTIO_NET_F_STATUS;
    }
    if (device_features & VIRTIO_NET_F_MTU) {
        driver_features |= VIRTIO_NET_F_MTU;
    }
    
    g_net_device->features = driver_features;
    
    /* Write accepted features */
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 0);
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DRIVER_FEATURES, (uint32_t)(driver_features & 0xFFFFFFFF));
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DRIVER_FEATURES_SEL, 1);
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_DRIVER_FEATURES, (uint32_t)(driver_features >> 32));
    
    status |= VIRTIO_STATUS_FEATURES_OK;
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_STATUS, status);
    
    /* Verify features accepted */
    status = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_STATUS);
    if (!(status & VIRTIO_STATUS_FEATURES_OK)) {
        kfree(g_net_device);
        g_net_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Read device configuration */
    virtio_net_config_t *config = (virtio_net_config_t *)(g_net_device->base_addr + VIRTIO_MMIO_CONFIG);
    
    /* Read MAC address */
    if (g_net_device->features & VIRTIO_NET_F_MAC) {
        for (int i = 0; i < ETH_ALEN; i++) {
            g_net_device->mac[i] = ((volatile uint8_t *)config)[i];
        }
    } else {
        /* Generate random MAC (locally administered) */
        g_net_device->mac[0] = 0x52;
        g_net_device->mac[1] = 0x54;
        g_net_device->mac[2] = 0x00;
        g_net_device->mac[3] = 0x12;
        g_net_device->mac[4] = 0x34;
        g_net_device->mac[5] = 0x56;
    }
    
    /* Read MTU */
    if (g_net_device->features & VIRTIO_NET_F_MTU) {
        g_net_device->mtu = config->mtu;
    } else {
        g_net_device->mtu = ETH_MTU;
    }
    
    /* Read link status */
    if (g_net_device->features & VIRTIO_NET_F_STATUS) {
        g_net_device->link_up = (config->status & VIRTIO_NET_S_LINK_UP) ? 1 : 0;
    } else {
        g_net_device->link_up = 1;  /* Assume up if no status feature */
    }
    
    /* Get queue sizes */
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_QUEUE_SEL, VIRTIO_NET_QUEUE_RX);
    uint32_t rx_queue_max = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_QUEUE_SEL, VIRTIO_NET_QUEUE_TX);
    uint32_t tx_queue_max = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_QUEUE_NUM_MAX);
    
    uint32_t rx_queue_size = (rx_queue_max < VIRTIO_NET_QUEUE_SIZE) ? rx_queue_max : VIRTIO_NET_QUEUE_SIZE;
    uint32_t tx_queue_size = (tx_queue_max < VIRTIO_NET_QUEUE_SIZE) ? tx_queue_max : VIRTIO_NET_QUEUE_SIZE;
    
    /* Initialize RX queue */
    if (virtqueue_net_init(g_net_device, &g_net_device->rx_queue, VIRTIO_NET_QUEUE_RX, rx_queue_size) < 0) {
        kfree(g_net_device);
        g_net_device = NULL;
        return -1;
    }
    
    /* Initialize TX queue */
    if (virtqueue_net_init(g_net_device, &g_net_device->tx_queue, VIRTIO_NET_QUEUE_TX, tx_queue_size) < 0) {
        kfree(g_net_device);
        g_net_device = NULL;
        return -1;
    }
    
    /* Set DRIVER_OK */
    status |= VIRTIO_STATUS_DRIVER_OK;
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_STATUS, status);
    
    /* Verify device accepted */
    uint32_t final_status = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_STATUS);
    if (!(final_status & VIRTIO_STATUS_DRIVER_OK)) {
        kfree(g_net_device);
        g_net_device = NULL;
        RETURN_ERRNO(THUNDEROS_EVIRTIO_BADDEV);
    }
    
    /* Post initial RX buffers */
    post_rx_buffers(g_net_device);
    
    clear_errno();
    return 0;
}

/**
 * Send a network packet
 */
int virtio_net_send(const void *data, size_t len)
{
    if (!g_net_device) {
        RETURN_ERRNO(THUNDEROS_EVIRTIO_NODEV);
    }
    
    if (!data || len == 0 || len > VIRTIO_NET_MAX_PACKET_SIZE) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    virtqueue_net_t *vq = &g_net_device->tx_queue;
    
    /* Allocate descriptor for header + data */
    uint16_t hdr_desc_idx, data_desc_idx;
    if (virtqueue_alloc_desc(vq, &hdr_desc_idx) < 0) {
        g_net_device->tx_errors++;
        return -1;
    }
    if (virtqueue_alloc_desc(vq, &data_desc_idx) < 0) {
        virtqueue_free_desc(vq, hdr_desc_idx);
        g_net_device->tx_errors++;
        RETURN_ERRNO(THUNDEROS_EBUSY);
    }
    
    /* Allocate DMA buffer for header */
    dma_region_t *hdr_region = dma_alloc(sizeof(virtio_net_hdr_t), DMA_ZERO);
    if (!hdr_region) {
        virtqueue_free_desc(vq, hdr_desc_idx);
        virtqueue_free_desc(vq, data_desc_idx);
        g_net_device->tx_errors++;
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Allocate DMA buffer for data */
    dma_region_t *data_region = dma_alloc(len, 0);
    if (!data_region) {
        dma_free(hdr_region);
        virtqueue_free_desc(vq, hdr_desc_idx);
        virtqueue_free_desc(vq, data_desc_idx);
        g_net_device->tx_errors++;
        RETURN_ERRNO(THUNDEROS_ENOMEM);
    }
    
    /* Setup header (no offloading) */
    virtio_net_hdr_t *hdr = (virtio_net_hdr_t *)hdr_region->virt_addr;
    hdr->flags = 0;
    hdr->gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr->hdr_len = 0;
    hdr->gso_size = 0;
    hdr->csum_start = 0;
    hdr->csum_offset = 0;
    
    /* Copy data */
    uint8_t *dst = (uint8_t *)data_region->virt_addr;
    const uint8_t *src = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        dst[i] = src[i];
    }
    
    /* Setup header descriptor */
    vq->desc[hdr_desc_idx].addr = hdr_region->phys_addr;
    vq->desc[hdr_desc_idx].len = sizeof(virtio_net_hdr_t);
    vq->desc[hdr_desc_idx].flags = VIRTQ_DESC_F_NEXT;
    vq->desc[hdr_desc_idx].next = data_desc_idx;
    
    /* Setup data descriptor */
    vq->desc[data_desc_idx].addr = data_region->phys_addr;
    vq->desc[data_desc_idx].len = len;
    vq->desc[data_desc_idx].flags = 0;  /* Last descriptor, device reads */
    vq->desc[data_desc_idx].next = 0;
    
    write_barrier();
    
    /* Add to available ring and notify */
    virtqueue_add_to_avail(vq, hdr_desc_idx);
    virtqueue_notify(g_net_device, VIRTIO_NET_QUEUE_TX);
    
    /* Poll for completion (synchronous for now) */
    uint32_t timeout = VIRTIO_NET_TX_TIMEOUT;
    while (timeout > 0) {
        uint32_t int_status = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_INTERRUPT_STATUS);
        if (int_status) {
            VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
        }
        
        uint16_t used_idx;
        uint32_t used_len;
        if (virtqueue_get_used_buf(vq, &used_idx, &used_len) == 0) {
            /* Free descriptors and buffers */
            virtqueue_free_desc(vq, data_desc_idx);
            virtqueue_free_desc(vq, hdr_desc_idx);
            dma_free(hdr_region);
            dma_free(data_region);
            
            g_net_device->tx_packets++;
            g_net_device->tx_bytes += len;
            
            clear_errno();
            return len;
        }
        timeout--;
    }
    
    /* Timeout */
    virtqueue_free_desc(vq, data_desc_idx);
    virtqueue_free_desc(vq, hdr_desc_idx);
    dma_free(hdr_region);
    dma_free(data_region);
    g_net_device->tx_errors++;
    
    RETURN_ERRNO(THUNDEROS_EVIRTIO_TIMEOUT);
}

/**
 * Receive a network packet (polling)
 */
int virtio_net_recv(void *buffer, size_t max_len)
{
    if (!g_net_device) {
        RETURN_ERRNO(THUNDEROS_EVIRTIO_NODEV);
    }
    
    if (!buffer || max_len == 0) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    virtqueue_net_t *vq = &g_net_device->rx_queue;
    
    /* Check for and acknowledge interrupts */
    uint32_t int_status = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_INTERRUPT_STATUS);
    if (int_status) {
        VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
    }
    
    /* Check used ring for completed receives */
    uint16_t desc_idx;
    uint32_t used_len;
    if (virtqueue_get_used_buf(vq, &desc_idx, &used_len) < 0) {
        clear_errno();
        return 0;  /* No packets available */
    }
    
    /* Find which RX buffer this descriptor belongs to */
    virtio_net_rx_buf_t *rxbuf = NULL;
    for (int i = 0; i < VIRTIO_NET_RX_BUFFERS; i++) {
        if (g_net_device->rx_buffers[i].in_use &&
            g_net_device->rx_buffers[i].desc_idx == desc_idx) {
            rxbuf = &g_net_device->rx_buffers[i];
            break;
        }
    }
    
    if (!rxbuf) {
        /* Shouldn't happen, but handle gracefully */
        virtqueue_free_desc(vq, desc_idx);
        g_net_device->rx_errors++;
        RETURN_ERRNO(THUNDEROS_EIO);
    }
    
    /* Skip virtio header, copy data */
    size_t packet_len = used_len - sizeof(virtio_net_hdr_t);
    if (packet_len > max_len) {
        packet_len = max_len;
        g_net_device->rx_dropped++;
    }
    
    uint8_t *src = (uint8_t *)rxbuf->buffer + sizeof(virtio_net_hdr_t);
    uint8_t *dst = (uint8_t *)buffer;
    for (size_t i = 0; i < packet_len; i++) {
        dst[i] = src[i];
    }
    
    /* Mark buffer as free and repost */
    rxbuf->in_use = 0;
    virtqueue_free_desc(vq, desc_idx);
    post_rx_buffers(g_net_device);
    
    g_net_device->rx_packets++;
    g_net_device->rx_bytes += packet_len;
    
    clear_errno();
    return packet_len;
}

/**
 * Check if packets are available
 */
int virtio_net_poll(void)
{
    if (!g_net_device) {
        return 0;
    }
    
    virtqueue_net_t *vq = &g_net_device->rx_queue;
    read_barrier();
    return (vq->last_seen_used != vq->used->idx) ? 1 : 0;
}

/**
 * Get MAC address
 */
void virtio_net_get_mac(uint8_t *mac)
{
    if (g_net_device && mac) {
        for (int i = 0; i < ETH_ALEN; i++) {
            mac[i] = g_net_device->mac[i];
        }
    }
}

/**
 * Check link status
 */
int virtio_net_link_up(void)
{
    if (!g_net_device) {
        return 0;
    }
    
    /* Re-read status if feature supported */
    if (g_net_device->features & VIRTIO_NET_F_STATUS) {
        virtio_net_config_t *config = (virtio_net_config_t *)(g_net_device->base_addr + VIRTIO_MMIO_CONFIG);
        g_net_device->link_up = (config->status & VIRTIO_NET_S_LINK_UP) ? 1 : 0;
    }
    
    return g_net_device->link_up;
}

/**
 * Get statistics
 */
void virtio_net_get_stats(uint64_t *rx_packets, uint64_t *tx_packets,
                          uint64_t *rx_bytes, uint64_t *tx_bytes)
{
    if (!g_net_device) {
        return;
    }
    
    if (rx_packets) *rx_packets = g_net_device->rx_packets;
    if (tx_packets) *tx_packets = g_net_device->tx_packets;
    if (rx_bytes) *rx_bytes = g_net_device->rx_bytes;
    if (tx_bytes) *tx_bytes = g_net_device->tx_bytes;
}

/**
 * Interrupt handler
 */
void virtio_net_irq_handler(void)
{
    if (!g_net_device) {
        return;
    }
    
    /* Read and acknowledge interrupt */
    uint32_t int_status = VIRTIO_READ32(g_net_device, VIRTIO_MMIO_INTERRUPT_STATUS);
    VIRTIO_WRITE32(g_net_device, VIRTIO_MMIO_INTERRUPT_ACK, int_status);
    
    /* Repost any freed RX buffers */
    post_rx_buffers(g_net_device);
}

/**
 * Get device structure
 */
virtio_net_device_t *virtio_net_get_device(void)
{
    return g_net_device;
}

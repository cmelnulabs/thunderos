/**
 * VirtIO GPU Device Driver
 * 
 * Implementation of VirtIO GPU specification for graphics output.
 * Provides 2D framebuffer rendering with scanout support.
 * 
 * Reference: VirtIO Specification 1.1, GPU Device
 */

#ifndef VIRTIO_GPU_H
#define VIRTIO_GPU_H

#include <stdint.h>
#include <stddef.h>

/* VirtIO Device ID for GPU */
#define VIRTIO_DEVICE_ID_GPU            16

/* VirtIO GPU Feature Bits */
#define VIRTIO_GPU_F_VIRGL              (1ULL << 0)   /* 3D mode supported */
#define VIRTIO_GPU_F_EDID               (1ULL << 1)   /* EDID supported */
#define VIRTIO_GPU_F_RESOURCE_UUID      (1ULL << 2)   /* Resource UUID supported */
#define VIRTIO_GPU_F_RESOURCE_BLOB      (1ULL << 3)   /* Resource blob supported */
#define VIRTIO_GPU_F_CONTEXT_INIT       (1ULL << 4)   /* Context init supported */

/* VirtIO GPU Command Types */
#define VIRTIO_GPU_CMD_GET_DISPLAY_INFO         0x0100
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D       0x0101
#define VIRTIO_GPU_CMD_RESOURCE_UNREF           0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT              0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH           0x0104
#define VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D      0x0105
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING  0x0106
#define VIRTIO_GPU_CMD_RESOURCE_DETACH_BACKING  0x0107
#define VIRTIO_GPU_CMD_GET_CAPSET_INFO          0x0108
#define VIRTIO_GPU_CMD_GET_CAPSET               0x0109
#define VIRTIO_GPU_CMD_GET_EDID                 0x010A
#define VIRTIO_GPU_CMD_RESOURCE_ASSIGN_UUID     0x010B
#define VIRTIO_GPU_CMD_RESOURCE_CREATE_BLOB     0x010C
#define VIRTIO_GPU_CMD_SET_SCANOUT_BLOB         0x010D

/* Cursor commands */
#define VIRTIO_GPU_CMD_UPDATE_CURSOR            0x0300
#define VIRTIO_GPU_CMD_MOVE_CURSOR              0x0301

/* Response types */
#define VIRTIO_GPU_RESP_OK_NODATA               0x1100
#define VIRTIO_GPU_RESP_OK_DISPLAY_INFO         0x1101
#define VIRTIO_GPU_RESP_OK_CAPSET_INFO          0x1102
#define VIRTIO_GPU_RESP_OK_CAPSET               0x1103
#define VIRTIO_GPU_RESP_OK_EDID                 0x1104
#define VIRTIO_GPU_RESP_OK_RESOURCE_UUID        0x1105
#define VIRTIO_GPU_RESP_OK_MAP_INFO             0x1106

/* Error responses */
#define VIRTIO_GPU_RESP_ERR_UNSPEC              0x1200
#define VIRTIO_GPU_RESP_ERR_OUT_OF_MEMORY       0x1201
#define VIRTIO_GPU_RESP_ERR_INVALID_SCANOUT_ID  0x1202
#define VIRTIO_GPU_RESP_ERR_INVALID_RESOURCE_ID 0x1203
#define VIRTIO_GPU_RESP_ERR_INVALID_CONTEXT_ID  0x1204
#define VIRTIO_GPU_RESP_ERR_INVALID_PARAMETER   0x1205

/* Pixel formats */
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM        1
#define VIRTIO_GPU_FORMAT_B8G8R8X8_UNORM        2
#define VIRTIO_GPU_FORMAT_A8R8G8B8_UNORM        3
#define VIRTIO_GPU_FORMAT_X8R8G8B8_UNORM        4
#define VIRTIO_GPU_FORMAT_R8G8B8A8_UNORM        67
#define VIRTIO_GPU_FORMAT_X8B8G8R8_UNORM        68
#define VIRTIO_GPU_FORMAT_A8B8G8R8_UNORM        121
#define VIRTIO_GPU_FORMAT_R8G8B8X8_UNORM        134

/* Maximum scanouts (displays) */
#define VIRTIO_GPU_MAX_SCANOUTS                 16

/* VirtIO GPU Queue Indices */
#define VIRTIO_GPU_QUEUE_CONTROL                0
#define VIRTIO_GPU_QUEUE_CURSOR                 1

/* Default queue size */
#define VIRTIO_GPU_QUEUE_SIZE                   64

/* Default framebuffer dimensions */
#define VIRTIO_GPU_DEFAULT_WIDTH                800
#define VIRTIO_GPU_DEFAULT_HEIGHT               600

/**
 * VirtIO GPU Configuration Space
 */
typedef struct {
    uint32_t events_read;       /* Pending events to read */
    uint32_t events_clear;      /* Events to clear */
    uint32_t num_scanouts;      /* Number of scanouts */
    uint32_t num_capsets;       /* Number of capability sets */
} __attribute__((packed)) virtio_gpu_config_t;

/**
 * VirtIO GPU Control Header
 * All GPU commands start with this header
 */
typedef struct {
    uint32_t type;              /* Command type */
    uint32_t flags;             /* Flags */
    uint64_t fence_id;          /* Fence ID for synchronization */
    uint32_t ctx_id;            /* Context ID (3D only) */
    uint8_t ring_idx;           /* Ring index */
    uint8_t padding[3];         /* Padding */
} __attribute__((packed)) virtio_gpu_ctrl_hdr_t;

/**
 * Display rectangle
 */
typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute__((packed)) virtio_gpu_rect_t;

/**
 * Display info for a single scanout
 */
typedef struct {
    virtio_gpu_rect_t r;        /* Display rectangle */
    uint32_t enabled;           /* Whether scanout is enabled */
    uint32_t flags;             /* Flags */
} __attribute__((packed)) virtio_gpu_display_one_t;

/**
 * Response to GET_DISPLAY_INFO command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_display_one_t pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute__((packed)) virtio_gpu_resp_display_info_t;

/**
 * RESOURCE_CREATE_2D command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;       /* Resource ID to create */
    uint32_t format;            /* Pixel format */
    uint32_t width;             /* Width in pixels */
    uint32_t height;            /* Height in pixels */
} __attribute__((packed)) virtio_gpu_resource_create_2d_t;

/**
 * RESOURCE_UNREF command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;       /* Resource ID to destroy */
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_unref_t;

/**
 * SET_SCANOUT command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;        /* Scanout rectangle */
    uint32_t scanout_id;        /* Scanout index */
    uint32_t resource_id;       /* Resource ID to display */
} __attribute__((packed)) virtio_gpu_set_scanout_t;

/**
 * RESOURCE_FLUSH command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;        /* Region to flush */
    uint32_t resource_id;       /* Resource ID */
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_flush_t;

/**
 * TRANSFER_TO_HOST_2D command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    virtio_gpu_rect_t r;        /* Region to transfer */
    uint64_t offset;            /* Offset in backing storage */
    uint32_t resource_id;       /* Resource ID */
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_transfer_to_host_2d_t;

/**
 * Memory entry for resource backing
 */
typedef struct {
    uint64_t addr;              /* Physical address */
    uint32_t length;            /* Length in bytes */
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_mem_entry_t;

/**
 * RESOURCE_ATTACH_BACKING command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;       /* Resource ID */
    uint32_t nr_entries;        /* Number of memory entries */
    /* Followed by nr_entries virtio_gpu_mem_entry_t */
} __attribute__((packed)) virtio_gpu_resource_attach_backing_t;

/**
 * RESOURCE_DETACH_BACKING command
 */
typedef struct {
    virtio_gpu_ctrl_hdr_t hdr;
    uint32_t resource_id;       /* Resource ID */
    uint32_t padding;
} __attribute__((packed)) virtio_gpu_resource_detach_backing_t;

/**
 * VirtQueue Descriptor (same as VirtIO block)
 */
typedef struct {
    uint64_t addr;              /* Physical address */
    uint32_t len;               /* Length */
    uint16_t flags;             /* Flags */
    uint16_t next;              /* Next descriptor index */
} __attribute__((packed)) virtio_gpu_desc_t;

/**
 * VirtQueue Available Ring
 */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtio_gpu_avail_t;

/**
 * VirtQueue Used Element
 */
typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtio_gpu_used_elem_t;

/**
 * VirtQueue Used Ring
 */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtio_gpu_used_elem_t ring[];
} __attribute__((packed)) virtio_gpu_used_t;

/**
 * VirtQueue structure
 */
typedef struct {
    virtio_gpu_desc_t *desc;        /* Descriptor ring */
    virtio_gpu_avail_t *avail;      /* Available ring */
    virtio_gpu_used_t *used;        /* Used ring */
    uintptr_t desc_phys;            /* Physical address of descriptor ring */
    uintptr_t avail_phys;           /* Physical address of available ring */
    uintptr_t used_phys;            /* Physical address of used ring */
    uint32_t queue_size;            /* Queue size */
    uint16_t last_seen_used;        /* Last seen used index */
    uint16_t num_free;              /* Number of free descriptors */
    uint16_t free_head;             /* Head of free descriptor list */
} virtio_gpu_queue_t;

/**
 * VirtIO GPU Device structure
 */
typedef struct {
    uintptr_t base_addr;            /* MMIO base address */
    uint32_t irq;                   /* IRQ number */
    uint32_t version;               /* Device version */
    uint32_t device_id;             /* Device ID */
    uint32_t vendor_id;             /* Vendor ID */
    uint64_t features;              /* Negotiated features */
    
    /* Queues */
    virtio_gpu_queue_t controlq;    /* Control queue */
    virtio_gpu_queue_t cursorq;     /* Cursor queue */
    
    /* Display info */
    uint32_t num_scanouts;          /* Number of scanouts */
    virtio_gpu_display_one_t displays[VIRTIO_GPU_MAX_SCANOUTS];
    
    /* Framebuffer */
    uint32_t fb_width;              /* Framebuffer width */
    uint32_t fb_height;             /* Framebuffer height */
    uint32_t fb_format;             /* Pixel format */
    uint32_t *fb_pixels;            /* Framebuffer pixel data */
    uintptr_t fb_phys;              /* Physical address of framebuffer */
    size_t fb_size;                 /* Framebuffer size in bytes */
    uint32_t resource_id;           /* GPU resource ID for framebuffer */
    
    /* Statistics */
    uint32_t flush_count;           /* Number of flushes */
    uint32_t error_count;           /* Number of errors */
} virtio_gpu_device_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Initialize VirtIO GPU device
 * 
 * @param base_addr MMIO base address
 * @param irq IRQ number
 * @return 0 on success, -1 on error (errno set)
 */
int virtio_gpu_init(uintptr_t base_addr, uint32_t irq);

/**
 * Check if VirtIO GPU is available
 * 
 * @return 1 if available, 0 otherwise
 */
int virtio_gpu_available(void);

/**
 * Get display information
 * 
 * @param width Pointer to store width
 * @param height Pointer to store height
 * @return 0 on success, -1 on error
 */
int virtio_gpu_get_display_info(uint32_t *width, uint32_t *height);

/**
 * Set a pixel in the framebuffer
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param color ARGB color value
 */
void virtio_gpu_set_pixel(uint32_t x, uint32_t y, uint32_t color);

/**
 * Get a pixel from the framebuffer
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @return ARGB color value
 */
uint32_t virtio_gpu_get_pixel(uint32_t x, uint32_t y);

/**
 * Clear the framebuffer with a color
 * 
 * @param color ARGB color value
 */
void virtio_gpu_clear(uint32_t color);

/**
 * Flush framebuffer to display
 * 
 * @return 0 on success, -1 on error
 */
int virtio_gpu_flush(void);

/**
 * Flush a region of the framebuffer
 * 
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Width of region
 * @param height Height of region
 * @return 0 on success, -1 on error
 */
int virtio_gpu_flush_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height);

/**
 * Get framebuffer pointer for direct access
 * 
 * @return Pointer to framebuffer pixels, or NULL if not available
 */
uint32_t *virtio_gpu_get_framebuffer(void);

/**
 * Get framebuffer dimensions
 * 
 * @param width Pointer to store width
 * @param height Pointer to store height
 */
void virtio_gpu_get_dimensions(uint32_t *width, uint32_t *height);

/**
 * Shutdown VirtIO GPU device
 */
void virtio_gpu_shutdown(void);

#endif /* VIRTIO_GPU_H */

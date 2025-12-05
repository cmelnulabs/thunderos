/**
 * VirtIO Network Device Driver
 * 
 * Implementation of VirtIO network device specification for packet I/O.
 * Supports modern VirtIO (v1.0+) with MMIO interface.
 * 
 * Reference: VirtIO Specification 1.1, Network Device
 */

#ifndef VIRTIO_NET_H
#define VIRTIO_NET_H

#include <stdint.h>
#include <stddef.h>

/* VirtIO Device ID for Network */
#define VIRTIO_DEVICE_ID_NET            1

/* VirtIO Network Feature Bits */
#define VIRTIO_NET_F_CSUM               (1ULL << 0)   /* Host handles checksums */
#define VIRTIO_NET_F_GUEST_CSUM         (1ULL << 1)   /* Guest handles checksums */
#define VIRTIO_NET_F_CTRL_GUEST_OFFLOADS (1ULL << 2)  /* Dynamic offload config */
#define VIRTIO_NET_F_MTU                (1ULL << 3)   /* Initial MTU advice */
#define VIRTIO_NET_F_MAC                (1ULL << 5)   /* Device has given MAC */
#define VIRTIO_NET_F_GUEST_TSO4         (1ULL << 7)   /* Guest can use TSOv4 */
#define VIRTIO_NET_F_GUEST_TSO6         (1ULL << 8)   /* Guest can use TSOv6 */
#define VIRTIO_NET_F_GUEST_ECN          (1ULL << 9)   /* Guest can use TSO with ECN */
#define VIRTIO_NET_F_GUEST_UFO          (1ULL << 10)  /* Guest can use UFO */
#define VIRTIO_NET_F_HOST_TSO4          (1ULL << 11)  /* Host can use TSOv4 */
#define VIRTIO_NET_F_HOST_TSO6          (1ULL << 12)  /* Host can use TSOv6 */
#define VIRTIO_NET_F_HOST_ECN           (1ULL << 13)  /* Host can use TSO with ECN */
#define VIRTIO_NET_F_HOST_UFO           (1ULL << 14)  /* Host can use UFO */
#define VIRTIO_NET_F_MRG_RXBUF          (1ULL << 15)  /* Merge receive buffers */
#define VIRTIO_NET_F_STATUS             (1ULL << 16)  /* Config status field */
#define VIRTIO_NET_F_CTRL_VQ            (1ULL << 17)  /* Control virtqueue */
#define VIRTIO_NET_F_CTRL_RX            (1ULL << 18)  /* Control RX mode */
#define VIRTIO_NET_F_CTRL_VLAN          (1ULL << 19)  /* Control VLAN filtering */
#define VIRTIO_NET_F_GUEST_ANNOUNCE     (1ULL << 21)  /* Guest can send gratuitous */
#define VIRTIO_NET_F_MQ                 (1ULL << 22)  /* Multiqueue support */
#define VIRTIO_NET_F_CTRL_MAC_ADDR      (1ULL << 23)  /* Set MAC address */

/* VirtIO Network Status Bits */
#define VIRTIO_NET_S_LINK_UP            1             /* Link is up */
#define VIRTIO_NET_S_ANNOUNCE           2             /* Announcement needed */

/* VirtIO Network Header Flags */
#define VIRTIO_NET_HDR_F_NEEDS_CSUM     1             /* Needs checksum */
#define VIRTIO_NET_HDR_F_DATA_VALID     2             /* Data is validated */
#define VIRTIO_NET_HDR_F_RSC_INFO       4             /* RSC info in csum fields */

/* VirtIO Network GSO Types */
#define VIRTIO_NET_HDR_GSO_NONE         0             /* No GSO */
#define VIRTIO_NET_HDR_GSO_TCPV4        1             /* TCP/IPv4 GSO */
#define VIRTIO_NET_HDR_GSO_UDP          3             /* UDP GSO */
#define VIRTIO_NET_HDR_GSO_TCPV6        4             /* TCP/IPv6 GSO */
#define VIRTIO_NET_HDR_GSO_ECN          0x80          /* ECN bit for GSO */

/* VirtIO Network Queue Indices */
#define VIRTIO_NET_QUEUE_RX             0             /* Receive queue */
#define VIRTIO_NET_QUEUE_TX             1             /* Transmit queue */
#define VIRTIO_NET_QUEUE_CTRL           2             /* Control queue (if feature) */

/* Default queue size */
#define VIRTIO_NET_QUEUE_SIZE           256

/* Maximum packet size (MTU + headers) */
#define VIRTIO_NET_MAX_PACKET_SIZE      1526          /* ETH_FRAME_LEN + VLAN */

/* Number of RX buffers to keep posted */
#define VIRTIO_NET_RX_BUFFERS           64

/* Ethernet constants */
#define ETH_ALEN                        6             /* MAC address length */
#define ETH_HLEN                        14            /* Ethernet header length */
#define ETH_MTU                         1500          /* Default MTU */
#define ETH_FRAME_LEN                   (ETH_MTU + ETH_HLEN)

/**
 * VirtIO Network Header
 * Prepended to every packet in both directions
 * 
 * Note: Without VIRTIO_NET_F_MRG_RXBUF, this is 10 bytes.
 * With VIRTIO_NET_F_MRG_RXBUF, add 2 more bytes for num_buffers.
 */
typedef struct {
    uint8_t flags;              /* VIRTIO_NET_HDR_F_* flags */
    uint8_t gso_type;           /* GSO type (NONE for no GSO) */
    uint16_t hdr_len;           /* Ethernet + IP + TCP/UDP header length */
    uint16_t gso_size;          /* GSO segment size */
    uint16_t csum_start;        /* Checksum start offset */
    uint16_t csum_offset;       /* Checksum offset from csum_start */
    /* Note: num_buffers only present with MRG_RXBUF - we don't use it */
} __attribute__((packed)) virtio_net_hdr_t;

/**
 * VirtIO Network Configuration Space
 * Located at MMIO offset 0x100
 */
typedef struct {
    uint8_t mac[ETH_ALEN];      /* MAC address (if VIRTIO_NET_F_MAC) */
    uint16_t status;            /* Link status (if VIRTIO_NET_F_STATUS) */
    uint16_t max_virtqueue_pairs; /* Max TX/RX queue pairs (if VIRTIO_NET_F_MQ) */
    uint16_t mtu;               /* MTU (if VIRTIO_NET_F_MTU) */
} __attribute__((packed)) virtio_net_config_t;

/**
 * VirtQueue Descriptor (same as block device)
 */
typedef struct {
    uint64_t addr;              /* Physical address */
    uint32_t len;               /* Length */
    uint16_t flags;             /* Flags (VIRTQ_DESC_F_*) */
    uint16_t next;              /* Next descriptor index */
} __attribute__((packed)) virtq_net_desc_t;

/**
 * VirtQueue Available Ring
 */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
} __attribute__((packed)) virtq_net_avail_t;

/**
 * VirtQueue Used Element
 */
typedef struct {
    uint32_t id;
    uint32_t len;
} __attribute__((packed)) virtq_net_used_elem_t;

/**
 * VirtQueue Used Ring
 */
typedef struct {
    uint16_t flags;
    uint16_t idx;
    virtq_net_used_elem_t ring[];
} __attribute__((packed)) virtq_net_used_t;

/**
 * VirtQueue for Network
 */
typedef struct {
    uint32_t queue_size;
    uint16_t last_seen_used;
    
    /* DMA-allocated rings */
    virtq_net_desc_t *desc;
    virtq_net_avail_t *avail;
    virtq_net_used_t *used;
    
    /* Physical addresses */
    uintptr_t desc_phys;
    uintptr_t avail_phys;
    uintptr_t used_phys;
    
    /* Free descriptor tracking */
    uint16_t free_head;
    uint16_t num_free;
} virtqueue_net_t;

/**
 * RX Buffer tracking
 */
typedef struct {
    void *buffer;               /* Virtual address of buffer */
    uintptr_t buffer_phys;      /* Physical address */
    uint16_t desc_idx;          /* Descriptor index in RX queue */
    uint8_t in_use;             /* Buffer is in device */
} virtio_net_rx_buf_t;

/**
 * VirtIO Network Device
 */
typedef struct {
    uintptr_t base_addr;        /* MMIO base address */
    uint32_t irq;               /* Interrupt number */
    
    /* Device info */
    uint32_t device_id;
    uint32_t vendor_id;
    uint32_t version;
    uint64_t features;          /* Negotiated features */
    
    /* Network properties */
    uint8_t mac[ETH_ALEN];      /* MAC address */
    uint16_t mtu;               /* MTU */
    uint8_t link_up;            /* Link status */
    
    /* VirtQueues */
    virtqueue_net_t rx_queue;   /* Receive queue */
    virtqueue_net_t tx_queue;   /* Transmit queue */
    
    /* RX buffer pool */
    virtio_net_rx_buf_t rx_buffers[VIRTIO_NET_RX_BUFFERS];
    
    /* Statistics */
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
} virtio_net_device_t;

/* Function Prototypes */

/**
 * Initialize VirtIO network device driver
 * @param base_addr MMIO base address of the device
 * @param irq Interrupt number
 * @return 0 on success, negative on error
 */
int virtio_net_init(uintptr_t base_addr, uint32_t irq);

/**
 * Send a network packet
 * @param data Pointer to packet data (including Ethernet header)
 * @param len Length of packet in bytes
 * @return Number of bytes sent, or negative on error
 */
int virtio_net_send(const void *data, size_t len);

/**
 * Receive a network packet (polling)
 * @param buffer Buffer to receive packet into
 * @param max_len Maximum buffer length
 * @return Number of bytes received, 0 if no packet, negative on error
 */
int virtio_net_recv(void *buffer, size_t max_len);

/**
 * Check if packets are available to receive
 * @return 1 if packets available, 0 if not
 */
int virtio_net_poll(void);

/**
 * Get MAC address
 * @param mac Buffer to store MAC address (6 bytes)
 */
void virtio_net_get_mac(uint8_t *mac);

/**
 * Check if link is up
 * @return 1 if link up, 0 if down
 */
int virtio_net_link_up(void);

/**
 * Get network device statistics
 * @param rx_packets Pointer to store RX packet count (may be NULL)
 * @param tx_packets Pointer to store TX packet count (may be NULL)
 * @param rx_bytes Pointer to store RX byte count (may be NULL)
 * @param tx_bytes Pointer to store TX byte count (may be NULL)
 */
void virtio_net_get_stats(uint64_t *rx_packets, uint64_t *tx_packets,
                          uint64_t *rx_bytes, uint64_t *tx_bytes);

/**
 * VirtIO network interrupt handler
 */
void virtio_net_irq_handler(void);

/**
 * Get the global VirtIO network device
 * @return Pointer to device structure, or NULL if not initialized
 */
virtio_net_device_t *virtio_net_get_device(void);

#endif /* VIRTIO_NET_H */

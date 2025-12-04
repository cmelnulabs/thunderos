/**
 * Network Interface Layer
 * 
 * Provides network interface abstraction with IP configuration
 * and packet send/receive through the VirtIO-net driver.
 */

#ifndef NET_NETIF_H
#define NET_NETIF_H

#include <stdint.h>
#include <stddef.h>

/* Ethernet constants */
#define ETH_ALEN            6       /* MAC address length */
#define ETH_HLEN            14      /* Ethernet header length */
#define ETH_MTU             1500    /* Maximum transmission unit */
#define ETH_FRAME_MAX       1518    /* Max frame size (MTU + headers) */

/* Ethernet types (big-endian) */
#define ETH_TYPE_IP         0x0800  /* Internet Protocol */
#define ETH_TYPE_ARP        0x0806  /* Address Resolution Protocol */

/* IP address as 32-bit integer (network byte order) */
typedef uint32_t ip4_addr_t;

/* Helper macros for IP addresses */
#define IP4_ADDR(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(c) << 8) | (uint32_t)(d))

#define IP4_ADDR_A(addr)    (((addr) >> 24) & 0xFF)
#define IP4_ADDR_B(addr)    (((addr) >> 16) & 0xFF)
#define IP4_ADDR_C(addr)    (((addr) >> 8) & 0xFF)
#define IP4_ADDR_D(addr)    ((addr) & 0xFF)

/**
 * Ethernet header
 */
typedef struct {
    uint8_t dst[ETH_ALEN];      /* Destination MAC */
    uint8_t src[ETH_ALEN];      /* Source MAC */
    uint16_t ethertype;         /* Protocol type (big-endian) */
} __attribute__((packed)) eth_hdr_t;

/**
 * Network interface structure
 */
typedef struct {
    uint8_t mac[ETH_ALEN];      /* MAC address */
    ip4_addr_t ip_addr;         /* IP address */
    ip4_addr_t netmask;         /* Subnet mask */
    ip4_addr_t gateway;         /* Default gateway */
    uint8_t up;                 /* Interface is up */
} netif_t;

/* Global network interface */
extern netif_t g_netif;

/**
 * Initialize network interface
 * @return 0 on success, -1 on error
 */
int netif_init(void);

/**
 * Configure IP address
 * @param ip IP address
 * @param netmask Subnet mask
 * @param gateway Default gateway
 */
void netif_set_ip(ip4_addr_t ip, ip4_addr_t netmask, ip4_addr_t gateway);

/**
 * Send raw Ethernet frame
 * @param dst Destination MAC address
 * @param ethertype Protocol type
 * @param data Payload data
 * @param len Payload length
 * @return Bytes sent, or -1 on error
 */
int netif_send(const uint8_t *dst, uint16_t ethertype, const void *data, size_t len);

/**
 * Receive and process incoming packets (polling)
 * Should be called periodically to handle incoming traffic
 */
void netif_poll(void);

/**
 * Get interface MAC address
 */
void netif_get_mac(uint8_t *mac);

/**
 * Get interface IP address
 */
ip4_addr_t netif_get_ip(void);

/**
 * Byte order conversion (host to network / network to host)
 */
static inline uint16_t htons(uint16_t x) {
    return ((x & 0xFF) << 8) | ((x >> 8) & 0xFF);
}

static inline uint16_t ntohs(uint16_t x) {
    return htons(x);
}

static inline uint32_t htonl(uint32_t x) {
    return ((x & 0xFF) << 24) | ((x & 0xFF00) << 8) |
           ((x >> 8) & 0xFF00) | ((x >> 24) & 0xFF);
}

static inline uint32_t ntohl(uint32_t x) {
    return htonl(x);
}

#endif /* NET_NETIF_H */

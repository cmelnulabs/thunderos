/**
 * Address Resolution Protocol (ARP)
 * 
 * Maps IP addresses to MAC addresses for Ethernet.
 */

#ifndef NET_ARP_H
#define NET_ARP_H

#include <stdint.h>
#include <stddef.h>
#include <net/netif.h>

/* ARP hardware types */
#define ARP_HW_ETHERNET     1

/* ARP operation codes */
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

/* ARP cache size */
#define ARP_CACHE_SIZE      16

/* ARP timeout (ms) */
#define ARP_TIMEOUT_MS      5000
#define ARP_CACHE_TTL_MS    300000  /* 5 minutes */

/**
 * ARP header for Ethernet/IPv4
 */
typedef struct {
    uint16_t hw_type;           /* Hardware type (1 = Ethernet) */
    uint16_t proto_type;        /* Protocol type (0x0800 = IP) */
    uint8_t  hw_len;            /* Hardware address length (6) */
    uint8_t  proto_len;         /* Protocol address length (4) */
    uint16_t opcode;            /* Operation (1=request, 2=reply) */
    uint8_t  sender_mac[ETH_ALEN];  /* Sender MAC */
    uint32_t sender_ip;         /* Sender IP */
    uint8_t  target_mac[ETH_ALEN];  /* Target MAC */
    uint32_t target_ip;         /* Target IP */
} __attribute__((packed)) arp_hdr_t;

#define ARP_HEADER_LEN      28

/**
 * ARP cache entry
 */
typedef struct {
    ip4_addr_t ip;              /* IP address */
    uint8_t mac[ETH_ALEN];      /* MAC address */
    uint64_t timestamp;         /* When entry was created */
    uint8_t valid;              /* Entry is valid */
} arp_entry_t;

/**
 * Initialize ARP subsystem
 */
void arp_init(void);

/**
 * Lookup MAC address for IP
 * @param ip IP address to lookup
 * @param mac Buffer to store MAC address
 * @return 0 if found, -1 if not in cache
 */
int arp_lookup(ip4_addr_t ip, uint8_t *mac);

/**
 * Send ARP request for IP address
 * @param ip IP address to resolve
 * @return 0 on success, -1 on error
 */
int arp_request(ip4_addr_t ip);

/**
 * Resolve IP to MAC with blocking wait
 * @param ip IP address to resolve
 * @param mac Buffer to store MAC address
 * @param timeout_ms Maximum wait time
 * @return 0 if resolved, -1 on timeout
 */
int arp_resolve(ip4_addr_t ip, uint8_t *mac, uint32_t timeout_ms);

/**
 * Process received ARP packet
 * @param data ARP packet data
 * @param len Packet length
 */
void arp_recv(const void *data, size_t len);

/**
 * Add entry to ARP cache
 * @param ip IP address
 * @param mac MAC address
 */
void arp_cache_add(ip4_addr_t ip, const uint8_t *mac);

#endif /* NET_ARP_H */

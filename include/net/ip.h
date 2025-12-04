/**
 * Internet Protocol (IPv4)
 * 
 * Minimal IPv4 implementation for packet routing.
 */

#ifndef NET_IP_H
#define NET_IP_H

#include <stdint.h>
#include <stddef.h>
#include <net/netif.h>

/* IP protocol numbers */
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

/* IP header flags */
#define IP_FLAG_DF          0x4000  /* Don't fragment */
#define IP_FLAG_MF          0x2000  /* More fragments */

/**
 * IPv4 header (20 bytes minimum)
 */
typedef struct {
    uint8_t  version_ihl;       /* Version (4) and IHL (5) */
    uint8_t  tos;               /* Type of service */
    uint16_t total_len;         /* Total length (big-endian) */
    uint16_t id;                /* Identification */
    uint16_t frag_off;          /* Fragment offset and flags */
    uint8_t  ttl;               /* Time to live */
    uint8_t  protocol;          /* Protocol (ICMP=1, TCP=6, UDP=17) */
    uint16_t checksum;          /* Header checksum */
    uint32_t src_addr;          /* Source IP (big-endian) */
    uint32_t dst_addr;          /* Destination IP (big-endian) */
} __attribute__((packed)) ip_hdr_t;

#define IP_HEADER_LEN       20

/**
 * Calculate IP checksum
 * @param data Pointer to data
 * @param len Length in bytes
 * @return Checksum value
 */
uint16_t ip_checksum(const void *data, size_t len);

/**
 * Send IP packet
 * @param dst_ip Destination IP address
 * @param protocol IP protocol number
 * @param data Payload data
 * @param len Payload length
 * @return Bytes sent, or -1 on error
 */
int ip_send(ip4_addr_t dst_ip, uint8_t protocol, const void *data, size_t len);

/**
 * Process received IP packet
 * @param data Packet data (starting at IP header)
 * @param len Packet length
 */
void ip_recv(const void *data, size_t len);

#endif /* NET_IP_H */

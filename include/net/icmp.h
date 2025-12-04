/**
 * Internet Control Message Protocol (ICMP)
 * 
 * Minimal ICMP implementation for ping (echo request/reply).
 */

#ifndef NET_ICMP_H
#define NET_ICMP_H

#include <stdint.h>
#include <stddef.h>
#include <net/netif.h>

/* ICMP message types */
#define ICMP_TYPE_ECHO_REPLY        0
#define ICMP_TYPE_DEST_UNREACHABLE  3
#define ICMP_TYPE_ECHO_REQUEST      8
#define ICMP_TYPE_TIME_EXCEEDED     11

/* ICMP codes for destination unreachable */
#define ICMP_CODE_NET_UNREACHABLE   0
#define ICMP_CODE_HOST_UNREACHABLE  1
#define ICMP_CODE_PORT_UNREACHABLE  3

/**
 * ICMP header (8 bytes)
 */
typedef struct {
    uint8_t  type;              /* Message type */
    uint8_t  code;              /* Type-specific code */
    uint16_t checksum;          /* Checksum */
    uint16_t id;                /* Identifier (for echo) */
    uint16_t seq;               /* Sequence number (for echo) */
} __attribute__((packed)) icmp_hdr_t;

#define ICMP_HEADER_LEN     8

/**
 * Ping state for tracking outstanding requests
 */
typedef struct {
    uint16_t id;                /* Echo ID */
    uint16_t seq;               /* Current sequence number */
    ip4_addr_t target;          /* Target IP address */
    uint64_t send_time;         /* Time request was sent (ms) */
    uint8_t waiting;            /* Waiting for reply */
    uint8_t received;           /* Reply received */
    uint32_t rtt;               /* Round-trip time (ms) */
} ping_state_t;

/* Global ping state */
extern ping_state_t g_ping_state;

/**
 * Send ICMP echo request (ping)
 * @param dst_ip Destination IP address
 * @param id Echo identifier
 * @param seq Sequence number
 * @param data Optional payload data
 * @param len Payload length
 * @return 0 on success, -1 on error
 */
int icmp_send_echo_request(ip4_addr_t dst_ip, uint16_t id, uint16_t seq,
                           const void *data, size_t len);

/**
 * Process received ICMP packet
 * @param src_ip Source IP address
 * @param data ICMP packet data
 * @param len Packet length
 */
void icmp_recv(ip4_addr_t src_ip, const void *data, size_t len);

/**
 * High-level ping function
 * @param dst_ip Destination IP address
 * @return Round-trip time in ms, or -1 on timeout/error
 */
int ping(ip4_addr_t dst_ip);

/**
 * Check if ping reply was received
 * @return 1 if reply received, 0 if still waiting, -1 on timeout
 */
int ping_check(void);

#endif /* NET_ICMP_H */

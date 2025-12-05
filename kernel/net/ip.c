/*
 * Internet Protocol (IPv4)
 * 
 * Minimal IPv4 implementation for packet routing.
 */

#include <net/ip.h>
#include <net/netif.h>
#include <net/arp.h>
#include <net/icmp.h>
#include <kernel/errno.h>
#include <hal/hal_uart.h>

/* Constants */
#define IP_VERSION_IHL_DEFAULT  0x45    /* IPv4, IHL=5 (20 bytes, no options) */
#define IP_DEFAULT_TTL          64      /* Default time-to-live */
#define IP_BROADCAST            0xFFFFFFFF  /* Broadcast address */

/* IP identification counter */
static uint16_t ip_id = 0;

/**
 * Calculate IP/ICMP checksum
 * Uses one's complement sum of 16-bit words
 */
uint16_t ip_checksum(const void *data, size_t len)
{
    const uint16_t *ptr = (const uint16_t *)data;
    uint32_t sum = 0;
    
    /* Sum all 16-bit words */
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    /* Add odd byte if present */
    if (len > 0) {
        sum += *((const uint8_t *)ptr);
    }
    
    /* Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

/**
 * Send IP packet
 */
int ip_send(ip4_addr_t dst_ip, uint8_t protocol, const void *data, size_t len)
{
    if (!data || len == 0 || len > ETH_MTU - IP_HEADER_LEN) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Build packet buffer */
    uint8_t packet[ETH_MTU];
    ip_hdr_t *ip = (ip_hdr_t *)packet;
    
    /* Fill IP header */
    ip->version_ihl = IP_VERSION_IHL_DEFAULT;
    ip->tos = 0;
    ip->total_len = htons(IP_HEADER_LEN + len);
    ip->id = htons(ip_id++);
    ip->frag_off = htons(IP_FLAG_DF);  /* Don't fragment */
    ip->ttl = IP_DEFAULT_TTL;
    ip->protocol = protocol;
    ip->checksum = 0;
    ip->src_addr = htonl(g_netif.ip_addr);
    ip->dst_addr = htonl(dst_ip);
    
    /* Calculate header checksum */
    ip->checksum = ip_checksum(ip, IP_HEADER_LEN);
    
    /* Copy payload */
    uint8_t *payload = packet + IP_HEADER_LEN;
    const uint8_t *src = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        payload[i] = src[i];
    }
    
    /* Determine next-hop MAC address */
    uint8_t dst_mac[ETH_ALEN];
    ip4_addr_t next_hop;
    
    /* Check if destination is on local network */
    if ((dst_ip & g_netif.netmask) == (g_netif.ip_addr & g_netif.netmask)) {
        next_hop = dst_ip;
    } else {
        next_hop = g_netif.gateway;
    }
    
    /* Resolve MAC address via ARP */
    if (arp_resolve(next_hop, dst_mac, ARP_TIMEOUT_MS) < 0) {
        RETURN_ERRNO(THUNDEROS_EHOSTUNREACH);
    }
    
    /* Send Ethernet frame */
    return netif_send(dst_mac, ETH_TYPE_IP, packet, IP_HEADER_LEN + len);
}

/**
 * Process received IP packet
 */
void ip_recv(const void *data, size_t len)
{
    if (len < IP_HEADER_LEN) {
        return;
    }
    
    const ip_hdr_t *ip = (const ip_hdr_t *)data;
    
    /* Check version */
    if ((ip->version_ihl >> 4) != 4) {
        return;
    }
    
    /* Get header length */
    size_t ihl = (ip->version_ihl & 0x0F) * 4;
    if (ihl < IP_HEADER_LEN || ihl > len) {
        return;
    }
    
    /* Verify checksum */
    if (ip_checksum(ip, ihl) != 0) {
        return;
    }
    
    /* Get total length */
    size_t total_len = ntohs(ip->total_len);
    if (total_len > len) {
        return;
    }
    
    /* Check if packet is for us */
    ip4_addr_t dst = ntohl(ip->dst_addr);
    if (dst != g_netif.ip_addr && dst != IP_BROADCAST) {
        return;  /* Not for us */
    }
    
    /* Get source IP */
    ip4_addr_t src = ntohl(ip->src_addr);
    
    /* Dispatch to protocol handler */
    const uint8_t *payload = (const uint8_t *)data + ihl;
    size_t payload_len = total_len - ihl;
    
    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            icmp_recv(src, payload, payload_len);
            break;
        case IP_PROTO_TCP:
            /* TODO: TCP handling */
            break;
        case IP_PROTO_UDP:
            /* TODO: UDP handling */
            break;
        default:
            break;
    }
}

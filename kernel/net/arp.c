/*
 * Address Resolution Protocol (ARP)
 * 
 * Maps IP addresses to MAC addresses for Ethernet.
 */

#include <net/arp.h>
#include <net/netif.h>
#include <drivers/virtio_net.h>
#include <hal/hal_timer.h>
#include <kernel/errno.h>
#include <hal/hal_uart.h>

/* Constants */
#define TIMER_TICK_MS           100     /* Timer tick period in milliseconds */
#define ARP_POLL_DELAY          10000   /* Delay loop iterations between polls */

/* Convert timer ticks to milliseconds */
static inline uint64_t time_get_ms(void) {
    extern uint64_t hal_timer_get_ticks(void);
    return hal_timer_get_ticks() * TIMER_TICK_MS;
}

/* ARP cache */
static arp_entry_t arp_cache[ARP_CACHE_SIZE];

/* Broadcast MAC address */
static const uint8_t broadcast_mac[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/**
 * Initialize ARP subsystem
 */
void arp_init(void)
{
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        arp_cache[i].valid = 0;
    }
}

/**
 * Lookup MAC address in cache
 */
int arp_lookup(ip4_addr_t ip, uint8_t *mac)
{
    uint64_t now = time_get_ms();
    
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (arp_cache[i].valid && arp_cache[i].ip == ip) {
            /* Check if entry expired */
            if (now - arp_cache[i].timestamp > ARP_CACHE_TTL_MS) {
                arp_cache[i].valid = 0;
                return -1;
            }
            
            /* Copy MAC address */
            for (int j = 0; j < ETH_ALEN; j++) {
                mac[j] = arp_cache[i].mac[j];
            }
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * Add entry to ARP cache
 */
void arp_cache_add(ip4_addr_t ip, const uint8_t *mac)
{
    int slot = -1;
    uint64_t now = time_get_ms();
    uint64_t oldest = now;
    int oldest_slot = 0;
    
    /* Look for existing entry or empty slot */
    for (int i = 0; i < ARP_CACHE_SIZE; i++) {
        if (!arp_cache[i].valid) {
            slot = i;
            break;
        }
        if (arp_cache[i].ip == ip) {
            slot = i;
            break;
        }
        /* Track oldest entry for replacement */
        if (arp_cache[i].timestamp < oldest) {
            oldest = arp_cache[i].timestamp;
            oldest_slot = i;
        }
    }
    
    /* Use oldest slot if no empty slot found */
    if (slot < 0) {
        slot = oldest_slot;
    }
    
    /* Update entry */
    arp_cache[slot].ip = ip;
    for (int i = 0; i < ETH_ALEN; i++) {
        arp_cache[slot].mac[i] = mac[i];
    }
    arp_cache[slot].timestamp = now;
    arp_cache[slot].valid = 1;
}

/**
 * Send ARP request
 */
int arp_request(ip4_addr_t ip)
{
    arp_hdr_t arp;
    
    /* Build ARP request */
    arp.hw_type = htons(ARP_HW_ETHERNET);
    arp.proto_type = htons(ETH_TYPE_IP);
    arp.hw_len = ETH_ALEN;
    arp.proto_len = 4;
    arp.opcode = htons(ARP_OP_REQUEST);
    
    /* Sender (us) */
    for (int i = 0; i < ETH_ALEN; i++) {
        arp.sender_mac[i] = g_netif.mac[i];
    }
    arp.sender_ip = htonl(g_netif.ip_addr);
    
    /* Target (who we're looking for) */
    for (int i = 0; i < ETH_ALEN; i++) {
        arp.target_mac[i] = 0;  /* Unknown */
    }
    arp.target_ip = htonl(ip);
    
    /* Send as broadcast */
    return netif_send(broadcast_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/**
 * Resolve IP to MAC with blocking wait
 */
int arp_resolve(ip4_addr_t ip, uint8_t *mac, uint32_t timeout_ms)
{
    /* Check cache first */
    if (arp_lookup(ip, mac) == 0) {
        return 0;
    }
    
    /* Send ARP request */
    if (arp_request(ip) < 0) {
        return -1;
    }
    
    /* Poll for reply */
    uint64_t start = time_get_ms();
    while (time_get_ms() - start < timeout_ms) {
        netif_poll();
        
        if (arp_lookup(ip, mac) == 0) {
            return 0;
        }
        
        /* Small delay to avoid spinning too hard */
        for (volatile int delay = 0; delay < ARP_POLL_DELAY; delay++);
    }
    
    RETURN_ERRNO(THUNDEROS_ETIMEDOUT);
}

/**
 * Process received ARP packet
 */
void arp_recv(const void *data, size_t len)
{
    if (len < ARP_HEADER_LEN) {
        return;
    }
    
    const arp_hdr_t *arp = (const arp_hdr_t *)data;
    
    /* Validate ARP packet */
    if (ntohs(arp->hw_type) != ARP_HW_ETHERNET ||
        ntohs(arp->proto_type) != ETH_TYPE_IP ||
        arp->hw_len != ETH_ALEN ||
        arp->proto_len != 4) {
        return;
    }
    
    ip4_addr_t sender_ip = ntohl(arp->sender_ip);
    ip4_addr_t target_ip = ntohl(arp->target_ip);
    
    /* Always update cache with sender info */
    arp_cache_add(sender_ip, arp->sender_mac);
    
    uint16_t opcode = ntohs(arp->opcode);
    
    if (opcode == ARP_OP_REQUEST) {
        /* Is this request for us? */
        if (target_ip == g_netif.ip_addr) {
            /* Send ARP reply */
            arp_hdr_t reply;
            
            reply.hw_type = htons(ARP_HW_ETHERNET);
            reply.proto_type = htons(ETH_TYPE_IP);
            reply.hw_len = ETH_ALEN;
            reply.proto_len = 4;
            reply.opcode = htons(ARP_OP_REPLY);
            
            /* We are the sender now */
            for (int i = 0; i < ETH_ALEN; i++) {
                reply.sender_mac[i] = g_netif.mac[i];
            }
            reply.sender_ip = htonl(g_netif.ip_addr);
            
            /* Target is original sender */
            for (int i = 0; i < ETH_ALEN; i++) {
                reply.target_mac[i] = arp->sender_mac[i];
            }
            reply.target_ip = arp->sender_ip;
            
            netif_send(arp->sender_mac, ETH_TYPE_ARP, &reply, sizeof(reply));
        }
    }
    /* ARP_OP_REPLY is handled by cache update above */
}

/*
 * Internet Control Message Protocol (ICMP)
 * 
 * Implements ping (echo request/reply).
 */

#include <net/icmp.h>
#include <net/ip.h>
#include <net/netif.h>
#include <hal/hal_timer.h>
#include <kernel/errno.h>
#include <hal/hal_uart.h>

/* Use hal_timer_get_ticks for milliseconds (each tick = 100ms) */
static inline uint64_t time_get_ms(void) {
    extern uint64_t hal_timer_get_ticks(void);
    return hal_timer_get_ticks() * 100;
}

/* Global ping state */
ping_state_t g_ping_state;

/* Default ping payload */
static const char ping_payload[] = "ThunderOS ping";

/**
 * Send ICMP echo request
 */
int icmp_send_echo_request(ip4_addr_t dst_ip, uint16_t id, uint16_t seq,
                           const void *data, size_t len)
{
    /* Build ICMP packet */
    uint8_t packet[64];
    icmp_hdr_t *icmp = (icmp_hdr_t *)packet;
    
    icmp->type = ICMP_TYPE_ECHO_REQUEST;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    
    /* Copy payload */
    size_t payload_len = 0;
    if (data && len > 0) {
        uint8_t *payload = packet + ICMP_HEADER_LEN;
        const uint8_t *src = (const uint8_t *)data;
        payload_len = (len > sizeof(packet) - ICMP_HEADER_LEN) ? 
                      sizeof(packet) - ICMP_HEADER_LEN : len;
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] = src[i];
        }
    }
    
    /* Calculate checksum over entire ICMP message */
    icmp->checksum = ip_checksum(packet, ICMP_HEADER_LEN + payload_len);
    
    /* Send via IP */
    return ip_send(dst_ip, IP_PROTO_ICMP, packet, ICMP_HEADER_LEN + payload_len);
}

/**
 * Send ICMP echo reply
 */
static int icmp_send_echo_reply(ip4_addr_t dst_ip, uint16_t id, uint16_t seq,
                                const void *data, size_t len)
{
    /* Build ICMP packet */
    uint8_t packet[64];
    icmp_hdr_t *icmp = (icmp_hdr_t *)packet;
    
    icmp->type = ICMP_TYPE_ECHO_REPLY;
    icmp->code = 0;
    icmp->checksum = 0;
    icmp->id = htons(id);
    icmp->seq = htons(seq);
    
    /* Copy payload */
    size_t payload_len = 0;
    if (data && len > 0) {
        uint8_t *payload = packet + ICMP_HEADER_LEN;
        const uint8_t *src = (const uint8_t *)data;
        payload_len = (len > sizeof(packet) - ICMP_HEADER_LEN) ? 
                      sizeof(packet) - ICMP_HEADER_LEN : len;
        for (size_t i = 0; i < payload_len; i++) {
            payload[i] = src[i];
        }
    }
    
    /* Calculate checksum */
    icmp->checksum = ip_checksum(packet, ICMP_HEADER_LEN + payload_len);
    
    /* Send via IP */
    return ip_send(dst_ip, IP_PROTO_ICMP, packet, ICMP_HEADER_LEN + payload_len);
}

/**
 * Process received ICMP packet
 */
void icmp_recv(ip4_addr_t src_ip, const void *data, size_t len)
{
    if (len < ICMP_HEADER_LEN) {
        return;
    }
    
    const icmp_hdr_t *icmp = (const icmp_hdr_t *)data;
    
    /* Verify checksum */
    if (ip_checksum(data, len) != 0) {
        return;
    }
    
    switch (icmp->type) {
        case ICMP_TYPE_ECHO_REQUEST:
            /* Respond to ping */
            icmp_send_echo_reply(src_ip, ntohs(icmp->id), ntohs(icmp->seq),
                                (const uint8_t *)data + ICMP_HEADER_LEN,
                                len - ICMP_HEADER_LEN);
            break;
            
        case ICMP_TYPE_ECHO_REPLY:
            /* Check if this is reply to our ping */
            if (g_ping_state.waiting &&
                ntohs(icmp->id) == g_ping_state.id &&
                ntohs(icmp->seq) == g_ping_state.seq &&
                src_ip == g_ping_state.target) {
                
                g_ping_state.received = 1;
                g_ping_state.rtt = time_get_ms() - g_ping_state.send_time;
                g_ping_state.waiting = 0;
            }
            break;
            
        case ICMP_TYPE_DEST_UNREACHABLE:
        case ICMP_TYPE_TIME_EXCEEDED:
            /* Handle error messages if we're waiting for ping */
            if (g_ping_state.waiting) {
                g_ping_state.waiting = 0;
                g_ping_state.received = 0;
            }
            break;
            
        default:
            break;
    }
}

/**
 * High-level ping function
 */
int ping(ip4_addr_t dst_ip)
{
    static uint16_t ping_id = 0x1234;
    static uint16_t ping_seq = 0;
    
    /* Setup ping state */
    g_ping_state.id = ping_id;
    g_ping_state.seq = ping_seq++;
    g_ping_state.target = dst_ip;
    g_ping_state.send_time = time_get_ms();
    g_ping_state.waiting = 1;
    g_ping_state.received = 0;
    g_ping_state.rtt = 0;
    
    /* Send echo request */
    if (icmp_send_echo_request(dst_ip, g_ping_state.id, g_ping_state.seq,
                               ping_payload, sizeof(ping_payload) - 1) < 0) {
        g_ping_state.waiting = 0;
        return -1;
    }
    
    /* Poll for reply with timeout */
    uint64_t timeout = 5000;  /* 5 seconds */
    while (g_ping_state.waiting && 
           (time_get_ms() - g_ping_state.send_time) < timeout) {
        netif_poll();
        
        /* Small delay */
        for (volatile int i = 0; i < 1000; i++);
    }
    
    if (g_ping_state.received) {
        return g_ping_state.rtt;
    }
    
    RETURN_ERRNO(THUNDEROS_ETIMEDOUT);
}

/**
 * Check ping status (non-blocking)
 */
int ping_check(void)
{
    if (g_ping_state.received) {
        return 1;
    }
    if (!g_ping_state.waiting) {
        return -1;  /* Timeout or error */
    }
    
    /* Still waiting */
    netif_poll();
    
    /* Check timeout */
    if ((time_get_ms() - g_ping_state.send_time) > 5000) {
        g_ping_state.waiting = 0;
        return -1;
    }
    
    return 0;  /* Still waiting */
}

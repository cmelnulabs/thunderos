/*
 * Network Interface Layer
 * 
 * Provides network interface abstraction with Ethernet frame handling.
 */

#include <net/netif.h>
#include <net/ip.h>
#include <net/arp.h>
#include <drivers/virtio_net.h>
#include <hal/hal_uart.h>
#include <kernel/errno.h>

/* Global network interface */
netif_t g_netif;

/* Receive buffer */
static uint8_t rx_buffer[ETH_FRAME_MAX];

/**
 * Initialize network interface
 */
int netif_init(void)
{
    virtio_net_device_t *dev = virtio_net_get_device();
    if (!dev) {
        RETURN_ERRNO(THUNDEROS_EVIRTIO_NODEV);
    }
    
    /* Get MAC address from device */
    virtio_net_get_mac(g_netif.mac);
    
    /* Default IP configuration (can be changed later) */
    /* Using QEMU user-mode networking defaults */
    g_netif.ip_addr = IP4_ADDR(10, 0, 2, 15);
    g_netif.netmask = IP4_ADDR(255, 255, 255, 0);
    g_netif.gateway = IP4_ADDR(10, 0, 2, 2);
    
    g_netif.up = virtio_net_link_up() ? 1 : 0;
    
    /* Initialize ARP cache */
    arp_init();
    
    /* Pre-populate QEMU SLIRP gateway MAC address */
    /* QEMU SLIRP uses 52:55:0a:00:02:02 for 10.0.2.2 gateway */
    uint8_t gateway_mac[6] = {0x52, 0x55, 0x0a, 0x00, 0x02, 0x02};
    arp_cache_add(g_netif.gateway, gateway_mac);
    
    clear_errno();
    return 0;
}

/**
 * Configure IP address
 */
void netif_set_ip(ip4_addr_t ip, ip4_addr_t netmask, ip4_addr_t gateway)
{
    g_netif.ip_addr = ip;
    g_netif.netmask = netmask;
    g_netif.gateway = gateway;
}

/**
 * Get interface MAC address
 */
void netif_get_mac(uint8_t *mac)
{
    if (mac) {
        for (int i = 0; i < ETH_ALEN; i++) {
            mac[i] = g_netif.mac[i];
        }
    }
}

/**
 * Get interface IP address
 */
ip4_addr_t netif_get_ip(void)
{
    return g_netif.ip_addr;
}

/**
 * Send raw Ethernet frame
 */
int netif_send(const uint8_t *dst, uint16_t ethertype, const void *data, size_t len)
{
    if (!dst || !data || len == 0 || len > ETH_MTU) {
        RETURN_ERRNO(THUNDEROS_EINVAL);
    }
    
    /* Build Ethernet frame */
    uint8_t frame[ETH_FRAME_MAX];
    eth_hdr_t *eth = (eth_hdr_t *)frame;
    
    /* Set destination and source MAC */
    for (int i = 0; i < ETH_ALEN; i++) {
        eth->dst[i] = dst[i];
        eth->src[i] = g_netif.mac[i];
    }
    eth->ethertype = htons(ethertype);
    
    /* Copy payload */
    uint8_t *payload = frame + ETH_HLEN;
    const uint8_t *src = (const uint8_t *)data;
    for (size_t i = 0; i < len; i++) {
        payload[i] = src[i];
    }
    
    /* Send frame via VirtIO */
    int result = virtio_net_send(frame, ETH_HLEN + len);
    if (result < 0) {
        return -1;
    }
    
    clear_errno();
    return len;
}

/**
 * Receive and process incoming packets
 */
void netif_poll(void)
{
    /* Check for incoming packets */
    int len = virtio_net_recv(rx_buffer, sizeof(rx_buffer));
    if (len <= 0) {
        return;
    }
    
    /* Need at least Ethernet header */
    if (len < ETH_HLEN) {
        return;
    }
    
    eth_hdr_t *eth = (eth_hdr_t *)rx_buffer;
    uint16_t ethertype = ntohs(eth->ethertype);
    uint8_t *payload = rx_buffer + ETH_HLEN;
    size_t payload_len = len - ETH_HLEN;
    
    /* Dispatch based on ethertype */
    switch (ethertype) {
        case ETH_TYPE_IP:
            ip_recv(payload, payload_len);
            break;
        case ETH_TYPE_ARP:
            arp_recv(payload, payload_len);
            break;
        default:
            /* Unknown protocol, ignore */
            break;
    }
}

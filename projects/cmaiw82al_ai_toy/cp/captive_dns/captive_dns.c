/**
 * @file captive_dns.c
 * @brief Captive DNS service
 */

 #include <string.h>

#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/prot/dns.h"
#include "lwip/def.h"
#include "lwip/netif.h"
#include "lwip/inet.h"
#include "components/log.h"
#include "os/os.h"

#include "captive_dns.h"

#define TAG "dns"

#define LOGE(format, ...) BK_LOGE(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGI(format, ...) BK_LOGI(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGD(format, ...) BK_LOGD(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)
#define LOGW(format, ...) BK_LOGW(TAG, "[%s:%d] " format, __func__, __LINE__, ##__VA_ARGS__)

#define CONFIG_EXAMPLE_DNS  "192.168.188.1"

/* DNS protocol definitions - moved from header file */
#define MAX_DOMAIN_LEN      253
#define DNS_TTL             60
#define DNS_HEADER_LEN      12
#define DNS_FLAG1_QR        0x80 /* Query/Response */
#define DNS_FLAG1_RD        0x01 /* Recursion Desired */
#define DNS_FLAG1_AA        0x04 /* Authoritative Answer */
#define DNS_PORT            53

/* DNS protocol structures - moved from header file */
typedef struct __attribute__((packed)) {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} dns_header_t;

typedef struct __attribute__((packed)) {
    uint16_t name_ptr;
    uint16_t type;
    uint16_t class;
    uint32_t ttl;
    uint16_t rdlength;
    uint32_t addr;
} dns_answer_t;

typedef struct __attribute__((packed)) {
    uint16_t type;
    uint16_t class;
} dns_query_t;

static struct udp_pcb *dns_pcb = NULL;

static int parse_dns_query(const uint8_t *payload, uint16_t len, uint8_t *domain_buf) 
{
    if (len < DNS_HEADER_LEN + 5) return 0; 
    
    const uint8_t *ptr = payload + DNS_HEADER_LEN;
    const uint8_t *end = payload + len;
    uint8_t *buf_ptr = domain_buf;
    uint8_t label_len;

    while (ptr < end && (label_len = *ptr++) != 0) {
        if (label_len > 63 || (ptr + label_len) > end) {
            return 0;
        }

        *buf_ptr++ = label_len;
        memcpy(buf_ptr, ptr, label_len);
        buf_ptr += label_len;
        ptr += label_len;
    }

    if ((end - ptr) < 4) return 0;
    
    *buf_ptr = 0;

    return 1;
}

static void dns_recv_cb(void *arg, struct udp_pcb *pcb, struct pbuf *p, 
                            const ip_addr_t *addr, u16_t port) 
{
    LOGI("%s:%d\r\n", __func__, __LINE__);
    uint8_t *payload = (uint8_t *)p->payload;
    struct pbuf *resp = NULL;
    char domain[MAX_DOMAIN_LEN + 1];
    
    if (p->tot_len <= DNS_HEADER_LEN) goto cleanup;

    dns_header_t *hdr = (dns_header_t *)payload;
    if ((ntohs(hdr->flags) & 0x8000) || (ntohs(hdr->qdcount) != 1)) {
        goto cleanup;
    }
    
    if (!parse_dns_query(payload, p->tot_len, (uint8_t *)domain)) {
        goto cleanup;
    }

    uint16_t qname_len = strlen(domain)+2; // include length byte and null terminator
    uint16_t total_len = DNS_HEADER_LEN + qname_len + sizeof(dns_query_t) + sizeof(dns_answer_t);

    resp = pbuf_alloc(PBUF_TRANSPORT, total_len, PBUF_RAM);
    if (!resp) goto cleanup;

    dns_header_t *rhdr = (dns_header_t *)resp->payload;
    memset(rhdr, 0, DNS_HEADER_LEN);
    rhdr->id = hdr->id;
    rhdr->flags = htons(0x8100); // QR=1, AA=1
    rhdr->qdcount = htons(1);
    rhdr->ancount = htons(1);

    uint8_t *rptr = (uint8_t *)(rhdr + 1);
    memcpy(rptr, domain, strlen(domain));
    rptr += strlen(domain);
    *rptr++ = 0;

    dns_query_t *query = (dns_query_t *)rptr;
    query->type = htons(1); // A record
    query->class = htons(1); // IN class
    rptr += sizeof(dns_query_t);

    dns_answer_t *ans = (dns_answer_t *)rptr;
    ans->name_ptr = htons(0xC00C);
    ans->type = htons(1);
    ans->class = htons(1);
    ans->ttl = htonl(DNS_TTL);
    ans->rdlength = htons(4);

    ans->addr = inet_addr((char *)CONFIG_EXAMPLE_DNS);

    LOGI("%s:%d domain:%s,qname_len:%d\r\n", __func__, __LINE__, domain, qname_len);
    udp_sendto(pcb, resp, addr, port);

cleanup:
    if (resp) pbuf_free(resp);
    pbuf_free(p);
}

int captive_dns_init(void) 
{
    if (dns_pcb) {
        LOGW("DNS service already initialized\n");
        return 0;
    }

    dns_pcb = udp_new();
    if (!dns_pcb) {
        LOGE("Failed to create UDP PCB\n");
        return -1;
    }

    ip_set_option(dns_pcb, SOF_REUSEADDR);
    int ret = udp_bind(dns_pcb, IP_ADDR_ANY, DNS_PORT);
    if (ret != ERR_OK) {
        LOGE("Failed to bind to DNS port: %d\n", ret);
        udp_remove(dns_pcb);
        dns_pcb = NULL;
        return -1;
    }
    
    LOGI("DNS service bound to port %d\n", DNS_PORT);
    udp_recv(dns_pcb, dns_recv_cb, NULL);
    
    return 0;
}

int captive_dns_deinit(void) 
{
    if (!dns_pcb) {
        LOGW("DNS service not initialized\n");
        return 0;
    }

    udp_remove(dns_pcb);
    dns_pcb = NULL;
    LOGI("DNS service deinitialized\n");
    
    return 0;
}
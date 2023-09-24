/*
 * Copyright (c) 2021-2022 Baidu.com, Inc. All Rights Reserved.
 * Copyright (c) 2022-2023 Jianzhang Peng. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Author: Jianzhang Peng (pengjianzhang@baidu.com)
 *         Jianzhang Peng (pengjianzhang@gmail.com)
 */

#ifndef __IP_H
#define __IP_H

#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <rte_ip.h>

#define IP6_ADDR_SIZE   16

#define IP_FLAG_DF  htons(0x4000)

/*
 * The lower 32 bits represent an IPv6 address.
 * The IPv4 address is in the same position as the lower 32 bits of IPv6.
 * */
typedef struct {
    union {
        struct in6_addr in6;
        struct {
            uint32_t pad[3];
            uint32_t ip;
        };
    };
} ipaddr_t;

#define ip_hdr_get_addr_low32(iph, saddr, daddr) do {               \
    const struct ip6_hdr *ip6h = (const struct ip6_hdr *)iph;   \
                                                                \
    if (iph->version == 4) {                                    \
        saddr = iph->saddr;                                     \
        daddr = iph->daddr;                                     \
    } else {                                                    \
        saddr = ip6h->ip6_src.s6_addr32[3];                     \
        daddr = ip6h->ip6_dst.s6_addr32[3];                     \
    }                                                           \
} while (0)

static inline void ipaddr_join(const ipaddr_t *prefix, uint32_t last, ipaddr_t *addr)
{
    addr->in6 = prefix->in6;
    addr->ip = last;
}

#define ipaddr_last_byte(addr) ((addr).in6.s6_addr[15])
#define ipaddr_eq(addr0, addr1) (memcmp((const void*)(addr0), (const void*)addr1, sizeof(struct in6_addr)) == 0)

static inline void iph_swap_addr(struct iphdr *iph)
{
    uint32_t ip = iph->saddr;
    iph->saddr = iph->daddr;
    iph->daddr = ip;
}

static inline void ip6h_swap_addr(struct ip6_hdr *ip6h)
{
    struct in6_addr addr;

    addr = ip6h->ip6_src;
    ip6h->ip6_src = ip6h->ip6_dst;
    ip6h->ip6_dst = addr;
}

#define IPV4_STR(addr) \
    ((const unsigned char *)&(addr))[0], \
    ((const unsigned char *)&(addr))[1], \
    ((const unsigned char *)&(addr))[2], \
    ((const unsigned char *)&(addr))[3]
#define IPV4_FMT "%u.%u.%u.%u"

#define IPV6_STR(addr) \
    ntohs(((uint16_t*)&(addr))[0]), \
    ntohs(((uint16_t*)&(addr))[1]), \
    ntohs(((uint16_t*)&(addr))[2]), \
    ntohs(((uint16_t*)&(addr))[3]), \
    ntohs(((uint16_t*)&(addr))[4]), \
    ntohs(((uint16_t*)&(addr))[5]), \
    ntohs(((uint16_t*)&(addr))[6]), \
    ntohs(((uint16_t*)&(addr))[7])
#define IPV6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"

int ipaddr_init(ipaddr_t *ip, const char *str);
void ipaddr_inc(ipaddr_t *ip, uint32_t n);

#endif

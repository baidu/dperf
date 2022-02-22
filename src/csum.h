/*
 * Copyright (c) 2022 Baidu.com, Inc. All Rights Reserved.
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
 */

#ifndef __CUMS_H
#define __CUMS_H

#include <rte_version.h>
#include "mbuf.h"

static inline uint16_t csum_update_tcp_seq(uint16_t ocsum, uint32_t seq0, uint32_t seq1)
{
    uint32_t csum = 0;

    csum = (~(seq0 >> 16) & 0xFFFF )
           + ((~(seq0 & 0xFFFF)) & 0xFFFF)
           + (~(seq1 >> 16) & 0xFFFF )
           + ((~(seq1 & 0xFFFF)) & 0xFFFF)
           + ocsum;
    csum = (csum & 0xFFFF) + (csum >> 16);
    csum = (csum & 0xFFFF) + (csum >> 16);

    return csum;
}

static inline uint16_t csum_update(uint16_t ocsum, uint16_t oval, uint16_t nval)
{
    uint32_t csum = 0;

    csum = (~ocsum & 0xFFFF) + (~oval & 0xFFFF) + nval;
    csum = (csum >> 16) + (csum & 0xFFFF);
    csum += (csum >> 16);

    return ~csum;
}

#if RTE_VERSION < RTE_VERSION_NUM(19, 0, 0, 0)
#define RTE_IPV4_CKSUM(iph) rte_ipv4_cksum((struct ipv4_hdr*)iph)
#define RTE_IPV4_UDPTCP_CKSUM(iph, th) rte_ipv4_udptcp_cksum((const struct ipv4_hdr *)iph, th)
#define RTE_IPV6_UDPTCP_CKSUM(iph, th) rte_ipv6_udptcp_cksum((const struct ipv6_hdr *)iph, (const void *)th)
#else
#define RTE_IPV4_CKSUM(iph) rte_ipv4_cksum((const struct rte_ipv4_hdr *)iph)
#define RTE_IPV4_UDPTCP_CKSUM(iph, th) rte_ipv4_udptcp_cksum((const struct rte_ipv4_hdr *)iph, th)
#define RTE_IPV6_UDPTCP_CKSUM(iph, th) rte_ipv6_udptcp_cksum((const struct rte_ipv6_hdr *)iph, (const void *)th)
#endif

static inline void csum_ipv4(struct rte_mbuf *m, int offload)
{
    struct iphdr *iph = NULL;

    iph = mbuf_ip_hdr(m);
    iph->check = 0;
    if (offload != 0) {
        m->ol_flags |= PKT_TX_IP_CKSUM;
        m->l2_len = sizeof(struct eth_hdr);
        m->l3_len = sizeof(struct iphdr);
    } else {
        iph->check = RTE_IPV4_CKSUM(iph);
    }
}

static inline void csum_ip_offload(struct rte_mbuf *m)
{
    csum_ipv4(m, g_dev_tx_offload_ipv4_cksum);
}

static inline void csum_ip_compute(struct rte_mbuf *m)
{
    csum_ipv4(m, 0);
}

static inline void csum_offload_ip_tcpudp(struct rte_mbuf *m, uint64_t ol_flags)
{
    struct ip6_hdr *ip6h = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *th = NULL;

    iph = mbuf_ip_hdr(m);
    th = mbuf_tcp_hdr(m);
    m->l2_len = sizeof(struct eth_hdr);

    if (iph->version == 4) {
        m->l3_len = sizeof(struct iphdr);
        iph->ttl = DEFAULT_TTL;
        iph->check = 0;

        if (unlikely(g_dev_tx_offload_tcpudp_cksum == 0)) {
            th->th_sum = 0;
            th->th_sum = RTE_IPV4_UDPTCP_CKSUM(iph, th);
        } else {
            m->ol_flags = ol_flags | PKT_TX_IPV4;
        }

        if (g_dev_tx_offload_ipv4_cksum) {
            m->ol_flags |= PKT_TX_IP_CKSUM | PKT_TX_IPV4;
        } else {
            iph->check = RTE_IPV4_CKSUM(iph);
        }
    } else {
        ip6h = (struct ip6_hdr *)iph;
        ip6h->ip6_hops = DEFAULT_TTL;
        if (unlikely(g_dev_tx_offload_tcpudp_cksum == 0)) {
            th->th_sum = 0;
            th->th_sum = RTE_IPV6_UDPTCP_CKSUM(ip6h, th);
        } else {
            m->l3_len = sizeof(struct ip6_hdr);
            m->ol_flags = ol_flags | PKT_TX_IPV6;
        }
    }
}

struct work_space;
struct socket;
void csum_init_socket(struct work_space *ws, struct socket *sk);
int csum_check(struct rte_mbuf *m);

#endif

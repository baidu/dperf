/*
 * Copyright (c) 2021 Baidu.com, Inc. All Rights Reserved.
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

#ifndef __MBUF_H
#define __MBUF_H

#include "mbuf.h"

#include <rte_mbuf.h>
#include <rte_arp.h>
#include <pthread.h>

#include "arp.h"
#include "config.h"
#include "port.h"
#include "tcp.h"
#include "eth.h"
#include "ip.h"
#include "icmp.h"

#define mbuf_eth_hdr(m) rte_pktmbuf_mtod(m, struct eth_hdr *)
#define mbuf_arphdr(m) rte_pktmbuf_mtod_offset(m, struct arphdr*, sizeof(struct eth_hdr))
#define mbuf_ip_hdr(m) rte_pktmbuf_mtod_offset(m, struct iphdr*, sizeof(struct eth_hdr))
#define mbuf_icmp_hdr(m) rte_pktmbuf_mtod_offset(m, struct icmphdr*, sizeof(struct eth_hdr) + sizeof(struct iphdr))
#define mbuf_tcp_hdr(m) ({                      \
    struct tcphdr *th = NULL;                   \
    struct eth_hdr *eth = mbuf_eth_hdr(m);      \
    if (eth->type == htons(ETHER_TYPE_IPv4)) {  \
        th = rte_pktmbuf_mtod_offset(m, struct tcphdr*, sizeof(struct eth_hdr) + sizeof(struct iphdr));     \
    } else {    \
        th = rte_pktmbuf_mtod_offset(m, struct tcphdr*, sizeof(struct eth_hdr) + sizeof(struct ip6_hdr));   \
    }\
    th;})
#define mbuf_udp_hdr(m) (struct udphdr*)mbuf_tcp_hdr(m)

#define mbuf_ip6_hdr(m) rte_pktmbuf_mtod_offset(m, struct ip6_hdr*, sizeof(struct eth_hdr))
#define mbuf_icmp6_hdr(m) rte_pktmbuf_mtod_offset(m, struct icmp6_hdr*, sizeof(struct eth_hdr) + sizeof(struct ip6_hdr))

#define RTE_PKTMBUF_PUSH(m, type) (type *)rte_pktmbuf_append(m, sizeof(type))
#define mbuf_push_eth_hdr(m) RTE_PKTMBUF_PUSH(m, struct eth_hdr)
#define mbuf_push_arphdr(m) RTE_PKTMBUF_PUSH(m, struct arphdr)
#define mbuf_push_iphdr(m) RTE_PKTMBUF_PUSH(m, struct iphdr)
#define mbuf_push_ip6_hdr(m) RTE_PKTMBUF_PUSH(m, struct ip6_hdr)
#define mbuf_push_tcphdr(m) RTE_PKTMBUF_PUSH(m, struct tcphdr)
#define mbuf_push_data(m, size) (uint8_t*)rte_pktmbuf_append(m, (size))

static inline void mbuf_ip_csum(struct rte_mbuf *m, int offload)
{
    struct iphdr *iph = NULL;

    iph = mbuf_ip_hdr(m);
    iph->check = 0;
    if (offload == 0) {
        iph->check = RTE_IPV4_CKSUM(iph);
    }

    m->ol_flags |= PKT_TX_IP_CKSUM;
    m->l2_len = sizeof(struct eth_hdr);
    m->l3_len = sizeof(struct iphdr);
}

static inline void mbuf_ip_csum_offload(struct rte_mbuf *m)
{
    mbuf_ip_csum(m, g_dev_tx_offload_ipv4_cksum);
}

static inline void mbuf_ip_csum_compute(struct rte_mbuf *m)
{
    mbuf_ip_csum(m, 0);
}

static inline uint16_t mbuf_cal_pseudo_csum(uint8_t proto, uint32_t sip, uint32_t dip, uint16_t len)
{
    uint32_t csum = 0;

    csum = (sip & 0x0000ffffUL) + (sip >> 16);
    csum += (dip & 0x0000ffffUL) + (dip >> 16);

    csum += (uint16_t)proto << 8;
    csum += htons(len);

    csum = (csum & 0x0000ffffUL) + (csum >> 16);
    csum = (csum & 0x0000ffffUL) + (csum >> 16);

    return (uint16_t)csum;
}

static inline void mbuf_csum_offload(struct rte_mbuf *m, uint64_t ol_flags)
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
            th->th_sum = RTE_IPV6_UDPTCP_CKSUM(iph, th);
        } else {
            m->l3_len = sizeof(struct ip6_hdr);
            m->ol_flags = ol_flags | PKT_TX_IPV6;
        }
    }
}

static inline void mbuf_iptcp_csum_offload(struct rte_mbuf *m)
{
    mbuf_csum_offload(m, PKT_TX_TCP_CKSUM);
}

static inline void mbuf_ipudp_csum_offload(struct rte_mbuf *m)
{
    mbuf_csum_offload(m, PKT_TX_UDP_CKSUM);
}

static inline void mbuf_prefetch(struct rte_mbuf *m)
{
    rte_prefetch0(mbuf_eth_hdr(m));
}

void mbuf_log(struct rte_mbuf *m, const char *tag);

#ifdef DPERF_DEBUG
#define MBUF_LOG(m, tag) mbuf_log(m, tag)
#else
#define MBUF_LOG(m, tag)
#endif

int mbuf_pool_init(struct config *cfg);
struct rte_mempool *mbuf_pool_create(const char *str, uint16_t port_id, uint16_t queue_id);

struct mbuf_free_pool {
    int num;
    struct rte_mbuf *head;
};

extern __thread struct mbuf_free_pool g_mbuf_free_pool;

#define mbuf_free(m) rte_pktmbuf_free(m)

static inline void mbuf_free2(struct rte_mbuf *m)
{
    if (m) {
        m->next = g_mbuf_free_pool.head;
        g_mbuf_free_pool.head = m;
        g_mbuf_free_pool.num++;
        if (g_mbuf_free_pool.num >= 128) {
            rte_pktmbuf_free(m);
            g_mbuf_free_pool.head = NULL;
            g_mbuf_free_pool.num = 0;
        }
    }
}

#endif

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

#ifndef __MBUF_H
#define __MBUF_H

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


static inline void mbuf_prefetch(struct rte_mbuf *m)
{
    rte_prefetch0(mbuf_eth_hdr(m));
}

void mbuf_print(struct rte_mbuf *m, const char *tag);
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

bool mbuf_is_neigh(struct rte_mbuf *m);
struct rte_mbuf *mbuf_dup(struct rte_mbuf *m);

#endif

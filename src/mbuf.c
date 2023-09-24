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

#include "mbuf.h"

#include <rte_ethdev.h>
#include <pthread.h>

#include "port.h"
#include "work_space.h"
#include "icmp6.h"

#define NB_MBUF             (8192 * 8)

__thread struct mbuf_free_pool g_mbuf_free_pool;

struct rte_mempool *mbuf_pool_create(const char *str, uint16_t port_id, uint16_t queue_id)
{
    int socket_id = 0;
    char name[RTE_RING_NAMESIZE];
    struct rte_mempool *mbuf_pool = NULL;
    int mbuf_size = 0;

    socket_id = rte_eth_dev_socket_id(port_id);
    snprintf(name, RTE_RING_NAMESIZE, "%s_%d_%d", str, port_id, queue_id);

    if (g_config.jumbo) {
        mbuf_size = JUMBO_MBUF_SIZE;
    } else {
        mbuf_size = RTE_MBUF_DEFAULT_BUF_SIZE;
    }

    mbuf_pool = rte_pktmbuf_pool_create(name, NB_MBUF,
                RTE_MEMPOOL_CACHE_MAX_SIZE, 0, mbuf_size, socket_id);

    if (mbuf_pool == NULL) {
        printf("rte_pkt_pool_create error\n");
    }
    return mbuf_pool;
}

void mbuf_log(struct rte_mbuf *m, const char *tag)
{
    uint8_t flags = 0;
    uint8_t fin = 0;
    uint8_t syn = 0;
    uint8_t ack = 0;
    uint8_t push = 0;
    uint8_t rst = 0;
    int len = 0;
    struct eth_hdr *eh = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *th = NULL;
    struct ip6_hdr *ip6h = NULL;
    char smac[64];
    char dmac[64];
    FILE *log = g_work_space->log;

    eh = mbuf_eth_hdr(m);
    eth_addr_to_str(&eh->s_addr, smac);
    eth_addr_to_str(&eh->d_addr, dmac);
    if (eh->type == htons(ETHER_TYPE_IPv4)) {
        iph = mbuf_ip_hdr(m);
        if (iph->protocol == IPPROTO_TCP) {
            th = mbuf_tcp_hdr(m);
            flags = th->th_flags;
            fin = ((flags & TH_FIN) != 0);
            syn = ((flags & TH_SYN) != 0);
            ack = ((flags & TH_ACK) != 0);
            push = ((flags & TH_PUSH) != 0);
            rst = ((flags & TH_RST) != 0);
            len = ntohs(iph->tot_len) - 20 - th->th_off * 4;
            fprintf(log, "sec %lu ticks %lu %s mbuf: "
                    " %s -> %s "
                    IPV4_FMT ":%u -> " IPV4_FMT ":%u "
                    "version %d ihl %d tos %x ttl %d frg_off %x ip.id %u "
                    "syn %d fin %d push %d ack %d rst %d seq %u ack %u th_off %u iplen %d len = %d\n",
                    g_current_seconds, g_current_ticks, tag,
                    smac, dmac,
                    IPV4_STR(iph->saddr), ntohs(th->th_sport),
                    IPV4_STR(iph->daddr), ntohs(th->th_dport),
                    iph->version, iph->ihl, iph->tos, iph->ttl,iph->frag_off, ntohs(iph->id),
                    syn, fin, push, ack, rst,
                    ntohl(th->th_seq), ntohl(th->th_ack),
                    th->th_off, ntohs(iph->tot_len), len);
        } else {
            fprintf(log, "sec %lu ticks %lu %s muf: %s -> %s " IPV4_FMT " ->" IPV4_FMT " proto %u\n",
                g_current_seconds, g_current_ticks, tag,
                smac, dmac, IPV4_STR(iph->saddr), IPV4_STR(iph->daddr), iph->protocol);
        }
    } else if (eh->type == htons(ETHER_TYPE_IPv6)) {
        ip6h = mbuf_ip6_hdr(m);
        fprintf(log, "muf: %s -> %s " IPV6_FMT " ->" IPV6_FMT " proto %u\n",
            smac, dmac, IPV6_STR(ip6h->ip6_src), IPV6_STR(ip6h->ip6_dst), ip6h->ip6_nxt);
    } else if (eh->type == htons(ETHER_TYPE_ARP)) {
        fprintf(log, "muf: %s -> %s arp\n", smac, dmac);
    } else {
        fprintf(log, "muf: %s -> %s type %x\n", smac, dmac, ntohs(eh->type));
    }
}

void mbuf_print(struct rte_mbuf *m, const char *tag)
{
    FILE *log = g_work_space->log;

    g_work_space->log = stdout;
    mbuf_log(m, tag);
    g_work_space->log = log;
}

static inline void mbuf_copy(struct rte_mbuf *dst, struct rte_mbuf *src)
{
    uint8_t *data = NULL;
    uint8_t *data2 = NULL;
    uint32_t len = 0;

    data = (uint8_t *)mbuf_eth_hdr(src);
    len = rte_pktmbuf_data_len(src);
    data2 = mbuf_push_data(dst, len);
    memcpy(data2, data, len);
}

struct rte_mbuf *mbuf_dup(struct rte_mbuf *m)
{
    struct rte_mbuf *m2 = NULL;
    struct work_space *ws = g_work_space;
    m2 = work_space_alloc_mbuf(ws);
    if (m2 != NULL) {
        mbuf_copy(m2, m);
    }

    return m2;
}

bool mbuf_is_neigh(struct rte_mbuf *m)
{
    uint8_t proto = 0;
    struct eth_hdr *eth = NULL;
    struct ip6_hdr *ip6h = NULL;

    eth = mbuf_eth_hdr(m);
    if (eth->type == htons(ETHER_TYPE_ARP)) {
        return true;
    } else if (eth->type == htons(ETHER_TYPE_IPv6)) {
        ip6h = mbuf_ip6_hdr(m);
        proto = ip6h->ip6_nxt;
        if (proto == IPPROTO_ICMPV6) {
            return icmp6_is_neigh(m);
        }
    }

    return false;
}

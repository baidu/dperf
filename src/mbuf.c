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

#include "mbuf.h"

#include <rte_ethdev.h>
#include <pthread.h>

#include "port.h"
#include "work_space.h"

#define NB_MBUF             (8192 * 8)

__thread struct mbuf_free_pool g_mbuf_free_pool;

struct rte_mempool *mbuf_pool_create(const char *str, uint16_t port_id, uint16_t queue_id)
{
    int socket_id = 0;
    char name[RTE_RING_NAMESIZE];
    struct rte_mempool *mbuf_pool = NULL;

    socket_id = rte_eth_dev_socket_id(port_id);
    if (socket_id < 0) {
        printf("bad socket_id %d of port %u\n", socket_id, port_id);
        return NULL;
    }
    snprintf(name, RTE_RING_NAMESIZE, "%s_%d_%d", str, port_id, queue_id);

    mbuf_pool = rte_pktmbuf_pool_create(name, NB_MBUF,
                RTE_MEMPOOL_CACHE_MAX_SIZE, 0, RTE_MBUF_DEFAULT_BUF_SIZE, socket_id);

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

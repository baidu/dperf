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

#ifndef __LOOP_H
#define __LOOP_H

#include "cpuload.h"
#include "work_space.h"
#include "icmp6.h"
#include "lldp.h"
#include "kni.h"
#include "socket_timer.h"
#include <rte_ip_frag.h>

/* optimal value, don't change */
#define MBUF_PREFETCH_NUM 4

typedef void(*l4_input_t)(struct work_space *, struct rte_mbuf *);
typedef void(*l3_input_t)(struct work_space *, struct rte_mbuf *, l4_input_t, l4_input_t);
typedef int(*work_space_process_t)(struct work_space *);

static inline int vxlan_check(struct work_space *ws, struct vxlan_headers *vxhs)
{
    struct udphdr *uh = NULL;
    struct iphdr *iph = NULL;

    iph = &vxhs->iph;
    uh = &vxhs->uh;
    /* Consider that the VXLAN packet is legitimate, simply check. */
    if ((iph->protocol == IPPROTO_UDP) &&
        (iph->daddr == ws->vtep_ip) &&
        (uh->dest == htons(VXLAN_PORT)) &&
        (vxhs->vxh.vni == ws->vni)) {
        return 0;
    }

    return -1;
}

static inline void vxlan_input(struct work_space *ws, struct rte_mbuf *m,
    l4_input_t tcp_input, l4_input_t udp_input)
{
    uint8_t proto = 0;
    struct iphdr *iph = NULL;
    struct ip6_hdr *ip6h = NULL;
    struct eth_hdr *eth = NULL;
    struct vxlan_headers *vxhs = NULL;

    vxhs = (struct vxlan_headers *)mbuf_eth_hdr(m);
    net_stats_rx(m);
    if (vxlan_check(ws, vxhs) == 0) {
        rte_pktmbuf_adj(m, VXLAN_HEADERS_SIZE);
        eth = (struct eth_hdr *)(((uint8_t *)vxhs) + VXLAN_HEADERS_SIZE);
        iph = (struct iphdr *)(((uint8_t *)eth) + sizeof(struct eth_hdr));
        ip6h = (struct ip6_hdr *)iph;
        if ((eth->type == htons(ETHER_TYPE_IPv4)) && (!ws->ipv6)) {
            net_stats_tos_ipv4_rx(ws, iph);
            proto = iph->protocol;
        } else if ((eth->type == htons(ETHER_TYPE_IPv6)) && (ws->ipv6)) {
            net_stats_tos_ipv6_rx(ip6h);
            proto = ip6h->ip6_nxt;
        } else {
            goto out;
        }

        if (proto == IPPROTO_TCP) {
            net_stats_tcp_rx();
            return tcp_input(ws, m);
        } else if (likely(proto == IPPROTO_UDP)) {
            net_stats_udp_rx();
            return udp_input(ws, m);
        }
    } else if (vxhs->iph.protocol == IPPROTO_ICMP) {
        return icmp_process(ws, m);
    } else if (vxhs->eh.type == htons(ETHER_TYPE_ARP)) {
        return arp_process(ws, m);
    }

    if (ws->kni) {
        return kni_recv(ws, m);
    }

out:
    net_stats_other_rx();
    mbuf_free(m);
}

/* ipv4 packets entry */
static inline void ipv4_input(struct work_space *ws, struct rte_mbuf *m,
    l4_input_t tcp_input, l4_input_t udp_input)
{
    uint8_t proto = 0;
    struct eth_hdr *eth = NULL;
    struct iphdr *iph = NULL;

    net_stats_rx(m);
    eth = mbuf_eth_hdr(m);
    iph = mbuf_ip_hdr(m);
    if (likely(eth->type == htons(ETHER_TYPE_IPv4))) {
        if (unlikely(m->ol_flags & (RTE_MBUF_F_RX_IP_CKSUM_BAD | RTE_MBUF_F_RX_L4_CKSUM_BAD))) {
            net_stats_rx_bad();
            mbuf_free(m);
            return;
        }

        iph = mbuf_ip_hdr(m);
        proto = iph->protocol;
        /* don't process ip options */
        if (unlikely((iph->ihl != 5) || rte_ipv4_frag_pkt_is_fragmented((const struct rte_ipv4_hdr *)iph))) {
            net_stats_rx_bad();
            mbuf_free(m);
            return;
        }

        net_stats_tos_ipv4_rx(ws, iph);
        if (likely(proto == IPPROTO_TCP)) {
            net_stats_tcp_rx();
            return tcp_input(ws, m);
        } else if (likely(proto == IPPROTO_UDP)) {
            net_stats_udp_rx();
            return udp_input(ws, m);
        } else if (proto == IPPROTO_ICMP) {
            return icmp_process(ws, m);
        }
    } else if (eth->type == htons(ETHER_TYPE_ARP)) {
        return arp_process(ws, m);
    }

    if (ws->kni) {
        return kni_recv(ws, m);
    }

    net_stats_other_rx();
    mbuf_free(m);
}

/* ipv6 packets entry */
static inline void ipv6_input(struct work_space *ws, struct rte_mbuf *m,
    void (*tcp_input)(struct work_space *, struct rte_mbuf *),
    void (*udp_input)(struct work_space *, struct rte_mbuf *))
{
    uint8_t proto = 0;
    struct eth_hdr *eth = NULL;
    struct ip6_hdr *ip6h = NULL;

    net_stats_rx(m);
    eth = mbuf_eth_hdr(m);
    if (likely(eth->type == htons(ETHER_TYPE_IPv6))) {
        ip6h = mbuf_ip6_hdr(m);
        proto = ip6h->ip6_nxt;

        if (unlikely(m->ol_flags & (RTE_MBUF_F_RX_L4_CKSUM_BAD))) {
            net_stats_rx_bad();
            mbuf_free(m);
            return;
        }

        net_stats_tos_ipv6_rx(ip6h);
        if (likely(proto == IPPROTO_TCP)) {
            net_stats_tcp_rx();
            return tcp_input(ws, m);
        } else if (likely(proto == IPPROTO_UDP)) {
            net_stats_udp_rx();
            return udp_input(ws, m);
        } else if (proto == IPPROTO_ICMPV6) {
            return icmp6_process(ws, m);
        }
    }

    if (ws->kni) {
        return kni_recv(ws, m);
    }

    net_stats_other_rx();
    mbuf_free(m);
}

static inline int slow_timer_run(struct work_space *ws)
{
    struct tick_time *tt = &ws->time;
    uint64_t ms100 = 0;
    uint64_t seconds = 0;

    if (ws->kni) {
        kni_send(ws);
    }

    ms100 = tsc_time_go(&tt->ms100, tt->tsc);
    if (unlikely(ms100 > 0)) {
        /*
         * lacp need to call tx_burst every 100ms
         * let's send some lldp packets
         * */
        if (ws->lldp) {
            lldp_send(ws);
        }
        seconds = tsc_time_go(&tt->second, tt->tsc);
        if (unlikely(seconds > 0)) {
            socket_timeout_timer_expire(ws);
            net_stats_timer_handler(ws);
            if (ws->exit) {
                return -1;
            }
        }
    }

    return 0;
}

static inline int client_recv_mbuf(struct work_space *ws, l3_input_t l3_input,
    l4_input_t tcp_input, l4_input_t udp_input, work_space_process_t client_launch)
{
    int i = 0;
    int j = MBUF_PREFETCH_NUM;
    int nb_rx = 0;
    struct rte_mbuf **mbuf_rx = ws->mbuf_rx;
    uint16_t port = ws->port_id;
    uint16_t queue = ws->queue_id;

    nb_rx = rte_eth_rx_burst(port, queue, mbuf_rx, RX_BURST_MAX);
    if (nb_rx) {
        if (nb_rx > MBUF_PREFETCH_NUM) {
            for (i = 0; i < MBUF_PREFETCH_NUM; i++) {
                mbuf_prefetch(mbuf_rx[i]);
            }
        }

        for (i = 0; i < nb_rx; i++, j++) {
            l3_input(ws, mbuf_rx[i], tcp_input, udp_input);
            if (j < nb_rx) {
                mbuf_prefetch(mbuf_rx[j]);
            }
            /*
             * Launch connections as evenly as possible
             * Since we have processed a lot of packets, now we launch new connections.
             * */
            if (i % 64 == 0) {
                tick_time_update(&ws->time);
                client_launch(ws);
            }
        }
        return 1;
    }
    return 0;
}

static inline int server_recv_mbuf(struct work_space *ws, l3_input_t l3_input,
    l4_input_t tcp_input, l4_input_t udp_input)
{
    int i = 0;
    int j = MBUF_PREFETCH_NUM;
    int nb_rx = 0;
    struct rte_mbuf **mbuf_rx = ws->mbuf_rx;
    uint16_t port = ws->port_id;
    uint16_t queue = ws->queue_id;

    nb_rx = rte_eth_rx_burst(port, queue, mbuf_rx, RX_BURST_MAX);
    if (nb_rx) {
        if (nb_rx > MBUF_PREFETCH_NUM) {
            for (i = 0; i < MBUF_PREFETCH_NUM; i++) {
                mbuf_prefetch(mbuf_rx[i]);
            }
        }

        for (i = 0; i < nb_rx; i++, j++) {
            l3_input(ws, mbuf_rx[i], tcp_input, udp_input);
            if (j < nb_rx) {
                mbuf_prefetch(mbuf_rx[j]);
            }
        }
        return 1;
    }
    return 0;
}

static inline void server_loop(struct work_space *ws, l3_input_t l3_input,
    l4_input_t tcp_input, l4_input_t udp_input, work_space_process_t socket_timer_process)
{
    int work = 0;
    uint64_t ticks = 0;
    struct tick_time *tt = NULL;

    tt = &ws->time;
    while (1) {
        /* step 1. read and process mbufs */
        work += server_recv_mbuf(ws, l3_input, tcp_input, udp_input);

        /* step 2. process timer */
        tick_time_update(tt);
        ticks = tsc_time_go(&tt->tick, tt->tsc);
        CPULOAD_ADD_TSC(&ws->load, tt->tsc, work);
        if (unlikely(ticks > 0)) {
            work = 1;
            if (unlikely(slow_timer_run(ws) < 0)) {
                break;
            }
            socket_timer_process(ws);
            work_space_tx_flush(ws);
        }
    }
}

static inline void client_loop(struct work_space *ws, l3_input_t l3_input,
    l4_input_t tcp_input, l4_input_t udp_input, work_space_process_t socket_timer_process,
    work_space_process_t client_launch)
{
    int work = 0;
    uint64_t ticks = 0;
    struct tick_time *tt = NULL;

    tt = &ws->time;
    while (1) {
        /* step 1. read and process mbufs */
        work += client_recv_mbuf(ws, l3_input, tcp_input, udp_input, client_launch);

        /* step 2. process timer */
        tick_time_update(tt);
        CPULOAD_ADD_TSC(&ws->load, tt->tsc, work);
        work += client_launch(ws);
        ticks = tsc_time_go(&tt->tick, tt->tsc);
        if (unlikely(ticks > 0)) {
            if (ws->ack_delay.next) {
                tcp_ack_delay_flush(ws);
            }

            work = 1;
            if (unlikely(slow_timer_run(ws) < 0)) {
                break;
            }
            socket_timer_process(ws);
            tick_time_update(tt);
            client_launch(ws);
            work_space_tx_flush(ws);
        }
    }
}

#endif

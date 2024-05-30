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

#include "udp.h"
#include "tcp.h"
#include "config.h"
#include "mbuf.h"
#include "mbuf_cache.h"
#include "net_stats.h"
#include "socket.h"
#include "tcp.h"
#include "version.h"
#include "loop.h"
#include "socket_timer.h"
#include "csum.h"

static char g_udp_data[MBUF_DATA_SIZE] = "hello dperf!!\n";

void udp_set_payload(struct config *cfg, int page_size)
{
    config_set_payload(cfg, g_udp_data, page_size, 1);
}

static inline void udp_change_dipv6(struct work_space *ws, struct ip6_hdr *ip6h, struct udphdr *uh)
{
    ipaddr_t addr_old;
    ipaddr_t addr_new;
    struct ip_list *ip_list = NULL;

    ip_list = &ws->dip_list;
    addr_old.in6 = ip6h->ip6_dst;
    ip_list_get_next_ipv6(ip_list, &addr_new.in6);
    ip6h->ip6_dst = addr_new.in6;
    uh->check = csum_update_u128(uh->check, (uint32_t *)&addr_old, (uint32_t *)&addr_new);
}

static inline void udp_change_dipv4(struct work_space *ws, struct iphdr *iph, struct udphdr *uh)
{
    uint32_t addr_old = 0;
    uint32_t addr_new = 0;
    struct ip_list *ip_list = NULL;

    ip_list = &ws->dip_list;
    addr_old = iph->daddr;
    ip_list_get_next_ipv4(ip_list, &addr_new);
    iph->daddr = addr_new;
    iph->check = csum_update_u32(iph->check, addr_old, addr_new);
    uh->check = csum_update_u32(uh->check, addr_old, addr_new);
}

static inline void udp_change_dip(struct work_space *ws, struct iphdr *iph, struct udphdr *uh)
{
    if (ws->ipv6) {
        udp_change_dipv6(ws, (struct ip6_hdr *)iph, uh);
    } else {
        udp_change_dipv4(ws, iph, uh);
    }
}

static inline struct rte_mbuf *udp_new_packet(struct work_space *ws, struct socket *sk)
{
    struct rte_mbuf *m = NULL;
    struct iphdr *iph = NULL;
    struct udphdr *uh = NULL;
    struct ip6_hdr *ip6h = NULL;

    m = mbuf_cache_alloc(&ws->udp);
    if (unlikely(m == NULL)) {
        return NULL;
    }

    if (ws->vxlan) {
        iph = (struct iphdr *)((uint8_t *)mbuf_eth_hdr(m) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
        if (ws->ipv6) {
            ip6h = (struct ip6_hdr *)iph;
            uh = (struct udphdr *)((uint8_t *)ip6h + sizeof(struct ip6_hdr));
        } else {
            uh = (struct udphdr *)((uint8_t *)iph + sizeof(struct iphdr));
            iph->check = csum_update_u16(sk->csum_ip, 0, htons(ws->ip_id));
        }
    } else {
        iph = mbuf_ip_hdr(m);
        ip6h = (struct ip6_hdr *)iph;
        uh = mbuf_udp_hdr(m);
    }

    if (!ws->ipv6) {
        iph->id = htons(ws->ip_id++);
        iph->saddr = sk->laddr;
        iph->daddr = sk->faddr;
    } else {
        ip6h->ip6_src.s6_addr32[3] = sk->laddr;
        ip6h->ip6_dst.s6_addr32[3] = sk->faddr;
    }

    uh->source = sk->lport;
    uh->dest = sk->fport;
    uh->check = sk->csum_udp;

    /* only in client mode */
    if (ws->change_dip) {
        udp_change_dip(ws, iph, uh);
    }

    return m;
}

static inline struct rte_mbuf* udp_send(struct work_space *ws, struct socket *sk)
{
    struct rte_mbuf *m = NULL;

    m = udp_new_packet(ws, sk);
    if (m) {
        work_space_tx_send_udp(ws, m);
    }

    return m;
}

static inline void udp_retransmit_handler(__rte_unused struct work_space *ws, struct socket *sk)
{
    /* rss auto: this socket is closed by another worker */
    if (sk->laddr != 0) {
        net_stats_udp_rt();
    }

    socket_close(sk);
}

static inline void udp_send_request(struct work_space *ws, struct socket *sk)
{
    sk->state = SK_SYN_SENT;
    sk->keepalive_request_num++;
    udp_send(ws, sk);
}

static void udp_socket_keepalive_timer_handler(struct work_space *ws, struct socket *sk)
{
    int pipeline = g_config.pipeline;

    if (work_space_in_duration(ws)) {
        /* rss auto: this socket is closed by another worker */
        if (unlikely(sk->laddr == 0)) {
            socket_close(sk);
            return;
        }

        /* if not flood mode, let's check packet loss */
        if ((!ws->flood) && (sk->state == SK_SYN_SENT)) {
            net_stats_udp_rt();
        }

        do {
            udp_send_request(ws, sk);
            pipeline--;
        } while (pipeline > 0);
        if (g_config.keepalive_request_interval) {
            socket_start_keepalive_timer(sk, work_space_tsc(ws));
        }
    }
}

static void udp_client_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct iphdr *iph = mbuf_ip_hdr(m);
    struct tcphdr *th = mbuf_tcp_hdr(m);
    struct socket *sk = NULL;

    sk = socket_client_lookup(&ws->socket_table, iph, th);
    if (unlikely(sk == NULL)) {
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        goto out;
    }

    if (sk->state != SK_CLOSED) {
        sk->state = SK_SYN_RECEIVED;
    } else {
        goto out;
    }
    if (sk->keepalive == 0) {
        net_stats_rtt(ws, sk);
        socket_close(sk);
    } else if ((g_config.keepalive_request_interval == 0) && (!ws->stop)) {
        udp_send_request(ws, sk);
    }

    mbuf_free(m);
    return;

out:
    net_stats_udp_drop();
    mbuf_free(m);
}

static void udp_server_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct iphdr *iph = mbuf_ip_hdr(m);
    struct tcphdr *th = mbuf_tcp_hdr(m);
    struct socket *sk = NULL;

    sk = socket_server_lookup(&ws->socket_table, iph, th);
    if (unlikely(sk == NULL)) {
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        goto out;
    }

    udp_send(ws, sk);
    mbuf_free(m);
    return;

out:
    net_stats_udp_drop();
    mbuf_free(m);
}

static int udp_client_launch(struct work_space *ws)
{
    uint64_t i = 0;
    struct socket *sk = NULL;
    uint64_t num = 0;
    int pipeline = g_config.pipeline;

    num = work_space_client_launch_num(ws);
    for (i = 0; i < num; i++) {
        sk = socket_client_open(&ws->socket_table, work_space_tsc(ws));
        if (unlikely(sk == NULL)) {
            continue;
        }

        do {
            udp_send(ws, sk);
            pipeline--;
        } while (pipeline > 0);
        if (sk->keepalive) {
            if (g_config.keepalive_request_interval) {
                /* for rtt calculationn */
                socket_start_keepalive_timer(sk, work_space_tsc(ws));
            }
        } else if (ws->flood) {
            socket_close(sk);
        } else {
            socket_start_retransmit_timer_force(sk, work_space_tsc(ws));
        }
    }

    return num;
}

static inline int udp_client_socket_timer_process(struct work_space *ws)
{
    struct socket_timer *rt_timer = &g_retransmit_timer;
    struct socket_timer *kp_timer = &g_keepalive_timer;

    if (g_config.keepalive) {
        socket_timer_run(ws, kp_timer, g_config.keepalive_request_interval, udp_socket_keepalive_timer_handler);
    } else {
        socket_timer_run(ws, rt_timer, g_config.retransmit_timeout, udp_retransmit_handler);
    }
    return 0;
}

static inline int udp_server_socket_timer_process(__rte_unused struct work_space *ws)
{
    return 0;
}

static void udp_server_run_loop_ipv4(struct work_space *ws)
{
    server_loop(ws, ipv4_input, tcp_drop, udp_server_process, udp_server_socket_timer_process);
}

static void udp_server_run_loop_ipv6(struct work_space *ws)
{
    server_loop(ws, ipv6_input, tcp_drop, udp_server_process, udp_server_socket_timer_process);
}

static void udp_client_run_loop_ipv4(struct work_space *ws)
{
    client_loop(ws, ipv4_input, tcp_drop, udp_client_process, udp_client_socket_timer_process, udp_client_launch);
}

static void udp_client_run_loop_ipv6(struct work_space *ws)
{
    client_loop(ws, ipv6_input, tcp_drop, udp_client_process, udp_client_socket_timer_process, udp_client_launch);
}

static void udp_server_run_loop_vxlan(struct work_space *ws)
{
    server_loop(ws, vxlan_input, tcp_drop, udp_server_process, udp_server_socket_timer_process);
}

static void udp_client_run_loop_vxlan(struct work_space *ws)
{
    client_loop(ws, vxlan_input, tcp_drop, udp_client_process, udp_client_socket_timer_process, udp_client_launch);
}

int udp_init(struct work_space *ws)
{
    if (g_config.protocol != IPPROTO_UDP) {
        return 0;
    }

    if (g_config.server) {
        if (ws->vxlan) {
            ws->run_loop = udp_server_run_loop_vxlan;
        } else if (ws->ipv6) {
            ws->run_loop = udp_server_run_loop_ipv6;
        } else {
            ws->run_loop = udp_server_run_loop_ipv4;
        }
    } else {
        if (ws->vxlan) {
            ws->run_loop = udp_client_run_loop_vxlan;
        } else if (ws->ipv6) {
            ws->run_loop = udp_client_run_loop_ipv6;
        } else {
            ws->run_loop = udp_client_run_loop_ipv4;
        }
    }

    return mbuf_cache_init_udp(&ws->udp, ws, "udp", g_udp_data);
}

void udp_drop(__rte_unused struct work_space *ws, struct rte_mbuf *m)
{
    if (m) {
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop");
        net_stats_udp_drop();
        mbuf_free2(m);
    }
}

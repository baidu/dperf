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

static char g_udp_data[MBUF_DATA_SIZE] = "hello dperf!!\n";

void udp_set_payload(int page_size)
{
    int i = 0;

    if (page_size == 0) {
        return;
    }

    for (i = 0; i < page_size; i++) {
        g_udp_data[i] = 'a';
    }

    if (page_size > 1) {
        g_udp_data[page_size - 1] = '\n';
    }
    g_udp_data[page_size] = 0;
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
            iph->check = csum_update(sk->csum_ip, 0, htons(ws->ip_id));
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

static void udp_socket_keepalive_timer_handler(struct work_space *ws, struct socket *sk)
{
    if (ws->server == 0) {
        if (work_space_in_duration(ws)) {
            udp_send(ws, sk);
            socket_start_keepalive_timer(sk, work_space_ticks(ws));
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
        if (ws->kni) {
            return kni_recv(ws, m);
        }
        goto out;
    }

    if (sk->keepalive == 0) {
        socket_close(sk);
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
        if (ws->kni) {
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
    bool flood = false;
    uint64_t i = 0;
    struct socket *sk = NULL;
    uint64_t num = 0;

    flood = g_config.flood;
    num = work_space_client_launch_num(ws);
    for (i = 0; i < num; i++) {
        sk = socket_client_open(&ws->socket_table);
        if (unlikely(sk == NULL)) {
            continue;
        }

        udp_send(ws, sk);
        if (sk->keepalive) {
            socket_start_keepalive_timer(sk, work_space_ticks(ws));
        } else if (flood) {
            socket_close(sk);
        }
    }

    return num;
}

static inline int udp_client_socket_timer_process(struct work_space *ws)
{
    struct socket_timer *kp_timer = &g_keepalive_timer;

    socket_timer_run(ws, kp_timer, g_config.keepalive_request_interval, udp_socket_keepalive_timer_handler);
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
        if (ws->kni) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop");
        net_stats_udp_drop();
        mbuf_free2(m);
    }
}

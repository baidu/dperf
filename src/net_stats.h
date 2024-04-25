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

#ifndef __NET_STATS_H
#define __NET_STATS_H

#include <stdint.h>
#include <stdio.h>

struct net_stats {
    /* Increasing */

    /* interface */
    uint64_t pkt_rx;
    uint64_t pkt_tx;
    uint64_t byte_rx;
    uint64_t byte_tx;

    uint64_t tx_drop;
    uint64_t pkt_lost;
    uint64_t rx_bad;

    uint64_t tos_rx;

    /* socket */
    uint64_t socket_dup;
    uint64_t socket_open;
    uint64_t socket_close;
    uint64_t socket_error;

    /* tcp */
    uint64_t tcp_rx;
    uint64_t tcp_tx;

    uint64_t syn_rx;
    uint64_t syn_tx;

    uint64_t fin_rx;
    uint64_t fin_tx;

    uint64_t rst_rx;
    uint64_t rst_tx;

    uint64_t syn_rt;
    uint64_t fin_rt;
    uint64_t ack_rt;
    uint64_t push_rt;
    uint64_t ack_dup;

    uint64_t http_2xx;
    uint64_t tcp_req;
    uint64_t http_get;
    uint64_t http_post;
    uint64_t tcp_rsp;
    uint64_t http_error;

    uint64_t tcp_drop;

    /* udp */
    uint64_t udp_rx;
    uint64_t udp_tx;
    uint64_t udp_rt;
    uint64_t udp_drop;

    /* arp */
    uint64_t arp_rx;
    uint64_t arp_tx;

    /* icmp and icmp6 */
    uint64_t icmp_rx;
    uint64_t icmp_tx;

    /* kni */
    uint64_t kni_rx;
    uint64_t kni_tx;

    uint64_t other_rx;

    /* mutable  */
    uint64_t mutable_start[0];

    uint64_t rtt_tsc;
    uint64_t rtt_num;

    uint64_t cpusage;
    uint64_t socket_current;
};

struct work_space;
void net_stats_init(struct work_space *ws);
void net_stats_timer_handler(struct work_space *ws);
void net_stats_print_total(FILE *fp);
void net_stats_print_speed(FILE *fp, int seconds);
extern __thread struct net_stats g_net_stats;
#define net_stats_socket_dup()      do {g_net_stats.socket_dup++;\
                                        g_net_stats.socket_open++; g_net_stats.socket_current++;} while (0)
#define net_stats_socket_error()    do {g_net_stats.socket_error++;} while (0)
#define net_stats_socket_open()     do {g_net_stats.socket_open++; g_net_stats.socket_current++;} while (0)
#define net_stats_socket_close()    do {g_net_stats.socket_close++; g_net_stats.socket_current--;} while (0)
#define net_stats_tx_drop(n)        do {g_net_stats.tx_drop += (n);} while (0)
#define net_stats_rx_bad()          do {g_net_stats.rx_bad++;} while (0)

#define net_stats_udp_rt()          do {g_net_stats.udp_rt++;} while (0)
#define net_stats_syn_rt()          do {g_net_stats.syn_rt++;} while (0)
#define net_stats_fin_rt()          do {g_net_stats.fin_rt++;} while (0)
#define net_stats_ack_rt()          do {g_net_stats.ack_rt++;} while (0)
#define net_stats_push_rt()         do {g_net_stats.push_rt++;} while (0)
#define net_stats_ack_dup()         do {g_net_stats.ack_dup++;} while (0)

#define net_stats_pkt_lost()        do {g_net_stats.pkt_lost++;} while (0)

#define net_stats_tcp_req()         do {g_net_stats.tcp_req++;} while (0)
#define net_stats_tcp_rsp()         do {g_net_stats.tcp_rsp++;} while (0)
#define net_stats_http_2xx()        do {g_net_stats.http_2xx++;} while (0)
#define net_stats_http_error()      do {g_net_stats.http_error++;} while (0)
#define net_stats_http_get()        do {g_net_stats.http_get++;} while (0)
#define net_stats_http_post()       do {g_net_stats.http_post++;} while (0)
#define net_stats_fin_rx()          do {g_net_stats.fin_rx++;} while (0)
#define net_stats_fin_tx()          do {g_net_stats.fin_tx++;} while (0)
#define net_stats_syn_rx()          do {g_net_stats.syn_rx++;} while (0)
#define net_stats_syn_tx()          do {g_net_stats.syn_tx++;} while (0)
#define net_stats_rst_rx()          do {g_net_stats.rst_rx++;} while (0)
#define net_stats_rst_tx()          do {g_net_stats.rst_tx++;} while (0)
#define net_stats_tcp_drop()        do {g_net_stats.tcp_drop++;} while (0)
#define net_stats_udp_drop()        do {g_net_stats.udp_drop++;} while (0)
#define net_stats_arp_rx()          do {g_net_stats.arp_rx++;} while (0)
#define net_stats_arp_tx()          do {g_net_stats.arp_tx++;} while (0)
#define net_stats_icmp_rx()         do {g_net_stats.icmp_rx++;} while (0)
#define net_stats_icmp_tx()         do {g_net_stats.icmp_tx++;} while (0)
#define net_stats_kni_rx()          do {g_net_stats.kni_rx++;} while (0)
#define net_stats_kni_tx()          do {g_net_stats.kni_tx++;} while (0)
#define net_stats_tcp_rx()          do {g_net_stats.tcp_rx++;} while (0)
#define net_stats_tcp_tx()          do {g_net_stats.tcp_tx++;} while (0)
#define net_stats_udp_rx()          do {g_net_stats.udp_rx++;} while (0)
#define net_stats_udp_tx()          do {g_net_stats.udp_tx++;} while (0)
#define net_stats_other_rx()        do {g_net_stats.other_rx++;} while (0)
#define net_stats_rx(m)             do {                                                \
                                        g_net_stats.pkt_rx++;                           \
                                        g_net_stats.byte_rx += rte_pktmbuf_data_len(m); \
                                    } while (0)

#define net_stats_tx(m)             do {                                                \
                                        g_net_stats.pkt_tx++;                           \
                                        g_net_stats.byte_tx += rte_pktmbuf_data_len(m); \
                                    } while (0)
#define net_stats_rtt(ws, sk)       do {                                                            \
                                        g_net_stats.rtt_num++;                                      \
                                        g_net_stats.rtt_tsc += work_space_tsc(ws) - sk->timer_tsc;  \
                                    } while (0)

#define net_stats_tos_ipv4_rx(ws, iph)  do {                                            \
                                        if (ws->tos && (ws->tos == iph->tos)) {         \
                                            g_net_stats.tos_rx++;                       \
                                        }                                               \
                                    } while (0)

#define net_stats_tos_ipv6_rx(ip6h) do {                                                \
                                        uint32_t mask = htonl(((uint32_t)0xffff) << 20);\
                                        if (ip6h->ip6_flow & mask) {                    \
                                            g_net_stats.tos_rx++;                       \
                                        }                                               \
                                    } while (0)

#endif

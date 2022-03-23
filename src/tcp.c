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

#include "tcp.h"
#include "udp.h"
#include "config.h"
#include "http.h"
#include "mbuf.h"
#include "net_stats.h"
#include "socket.h"
#include "version.h"
#include "socket_timer.h"
#include "loop.h"

#define tcp_seq_lt(seq0, seq1)    ((int)((seq0) - (seq1)) < 0)
#define tcp_seq_le(seq0, seq1)    ((int)((seq0) - (seq1)) <= 0)
#define tcp_seq_gt(seq0, seq1)    ((int)((seq0) - (seq1)) > 0)
#define tcp_seq_ge(seq0, seq1)    ((int)((seq0) - (seq1)) >= 0)

static inline void tcp_process_rst(struct socket *sk, struct rte_mbuf *m)
{
#ifdef DPERF_DEBUG
    if (sk->log) {
        SOCKET_LOG(sk, "rst");
        MBUF_LOG(m, "rst");
    }
#endif
    if (sk->state != SK_CLOSED) {
        socket_close(sk);
    }

    mbuf_free2(m);
}

static inline struct rte_mbuf *tcp_new_packet(struct work_space *ws, struct socket *sk, uint8_t tcp_flags)
{
    uint16_t csum_ip = 0;
    uint16_t csum_tcp = 0;
    uint16_t snd_una = 0;
    struct rte_mbuf *m = NULL;
    struct iphdr *iph = NULL;
    struct ip6_hdr *ip6h = NULL;
    struct tcphdr *th = NULL;
    struct mbuf_cache *p = NULL;

    if (tcp_flags & TH_SYN) {
        p = &ws->tcp_opt;
        csum_tcp = sk->csum_tcp_opt;
        csum_ip = sk->csum_ip_opt;
    } else if (tcp_flags & TH_PUSH) {
        p = &ws->tcp_data;
        csum_tcp = sk->csum_tcp_data;
        csum_ip = sk->csum_ip_data;
        snd_una = p->data.data_len;
    } else {
        p = &ws->tcp;
        csum_tcp = sk->csum_tcp;
        csum_ip = sk->csum_ip;
    }

    m = mbuf_cache_alloc(p);
    if (unlikely(m == NULL)) {
        return NULL;
    }

    if (tcp_flags & (TH_FIN | TH_SYN)) {
        snd_una++;
    }

    if (sk->snd_nxt == sk->snd_una) {
        sk->snd_una += snd_una;
    }

    /* update vxlan inner header */
    if (ws->vxlan) {
        iph = (struct iphdr *)((uint8_t *)mbuf_eth_hdr(m) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
        if (ws->ipv6) {
            ip6h = (struct ip6_hdr *)iph;
            th = (struct tcphdr *)((uint8_t *)ip6h + sizeof(struct ip6_hdr));
        } else {
            th = (struct tcphdr *)((uint8_t *)iph + sizeof(struct iphdr));
            iph->check = csum_update(csum_ip, 0, htons(ws->ip_id));
        }

        csum_tcp = csum_update_tcp_seq(csum_tcp, htonl(sk->snd_nxt), htonl(sk->rcv_nxt));
        csum_tcp = csum_update(csum_tcp, 0, htons(tcp_flags));
    } else {
        iph = mbuf_ip_hdr(m);
        th = mbuf_tcp_hdr(m);
    }

    if (!ws->ipv6) {
        iph->id = htons(ws->ip_id++);
        iph->saddr = sk->laddr;
        iph->daddr = sk->faddr;
    } else {
        ip6h = (struct ip6_hdr *)iph;
        ip6h->ip6_src.s6_addr32[3] = sk->laddr;
        ip6h->ip6_dst.s6_addr32[3] = sk->faddr;
    }

    th->th_sport = sk->lport;
    th->th_dport = sk->fport;
    th->th_flags = tcp_flags;
    th->th_seq = htonl(sk->snd_nxt);
    th->th_ack = htonl(sk->rcv_nxt);
    th->th_sum = csum_tcp;

#ifdef DPERF_DEBUG
    if ((tcp_flags & TH_SYN) && ((sk->lport == htons(80)) || (sk->fport == htons(80)))) {
        MBUF_LOG(m, "syn-tx");
        SOCKET_LOG(sk, "syn-tx");
    } else if ((sk->log) && ((sk->lport == htons(80)) || (sk->fport == htons(80)))) {
        MBUF_LOG(m, "tx");
        SOCKET_LOG(sk, "tx");
    }
#endif

    return m;
}

static inline void tcp_flags_rx_count(uint8_t tcp_flags)
{
    if (tcp_flags & TH_SYN) {
        net_stats_syn_rx();
    }

    if (tcp_flags & TH_FIN) {
        net_stats_fin_rx();
    }

    if (tcp_flags & TH_RST) {
        net_stats_rst_rx();
    }
}

static inline void tcp_flags_tx_count(uint8_t tcp_flags)
{
    if (tcp_flags & TH_SYN) {
        net_stats_syn_tx();
    }

    if (tcp_flags & TH_FIN) {
        net_stats_fin_tx();
    }

    if (tcp_flags & TH_RST) {
        net_stats_rst_tx();
    }
}

static struct rte_mbuf *tcp_reply(struct work_space *ws, struct socket *sk, uint8_t tcp_flags)
{
    struct rte_mbuf *m = NULL;

    sk->flags = tcp_flags;
    tcp_flags_tx_count(tcp_flags);

    if (tcp_flags & TH_PUSH) {
        if (g_config.server == 0) {
            net_stats_tcp_req();
            net_stats_http_get();
        } else {
            net_stats_tcp_rsp();
            net_stats_http_2xx();
        }
    }

    m = tcp_new_packet(ws, sk, tcp_flags);
    if (m) {
        work_space_tx_send_tcp(ws, m);
    }

    if ((sk->state != SK_CLOSED) && (tcp_flags & (TH_PUSH | TH_SYN | TH_FIN))) {
        socket_start_retransmit_timer(sk, work_space_ticks(ws));
    }

    return m;
}

extern void tcp_socket_send_rst(struct work_space *ws, struct socket *sk);
void tcp_socket_send_rst(struct work_space *ws, struct socket *sk)
{
    sk->snd_nxt = 0;
    sk->snd_una = 0;
    tcp_reply(ws, sk, TH_RST | TH_ACK);
    net_stats_socket_error();
    socket_close(sk);
}

static void tcp_rst_set_ip(struct iphdr *iph)
{
    uint32_t saddr = 0;
    uint32_t daddr = 0;

    daddr = iph->saddr;
    saddr = iph->daddr;

    memset(iph, 0, sizeof(struct iphdr));
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = g_config.tos;
    iph->tot_len = htons(40);
    iph->ttl = DEFAULT_TTL;
    iph->frag_off = IP_FLAG_DF;
    iph->protocol = IPPROTO_TCP;
    iph->saddr = saddr;
    iph->daddr = daddr;
}

static void tcp_rst_set_ipv6(struct ip6_hdr *ip6h)
{
    struct in6_addr ip6_src;
    struct in6_addr ip6_dst;

    ip6_src = ip6h->ip6_dst;
    ip6_dst = ip6h->ip6_src;

    memset(ip6h, 0, sizeof(struct ip6_hdr));
    ip6h->ip6_vfc = (6 << 4);
    ip6h->ip6_flow |= htonl(((uint32_t)g_config.tos) << 20);
    ip6h->ip6_hops = DEFAULT_TTL;
    ip6h->ip6_src = ip6_src;
    ip6h->ip6_dst = ip6_dst;
    ip6h->ip6_plen = htons(20);
    ip6h->ip6_nxt = IPPROTO_TCP;
}

static void tcp_rst_set_tcp(struct tcphdr *th)
{
    uint32_t seq = 0;
    uint16_t sport = 0;
    uint16_t dport = 0;

    sport = th->th_dport;
    dport = th->th_sport;
    seq = ntohl(th->th_seq);
    if (th->th_flags & TH_SYN) {
        seq++;
    }

    if (th->th_flags & TH_FIN) {
        seq++;
    }

    memset(th, 0, sizeof(struct tcphdr));
    th->th_sport = sport;
    th->th_dport = dport;
    th->th_seq = 0;
    th->th_ack = htonl(seq);
    th->th_off = 5;
    th->th_flags = TH_RST | TH_ACK;
}

static void tcp_reply_rst(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = NULL;
    struct iphdr *iph = NULL;
    struct tcphdr *th = NULL;

    /* not support rst yet */
    if (ws->vxlan) {
        net_stats_tcp_drop();
        mbuf_free2(m);
        return;
    }

    eth = mbuf_eth_hdr(m);
    iph = mbuf_ip_hdr(m);
    th = mbuf_tcp_hdr(m);

    eth_addr_swap(eth);
    if (iph->version == 4) {
        tcp_rst_set_ip(iph);
        tcp_rst_set_tcp(th);
        th->th_sum = RTE_IPV4_UDPTCP_CKSUM(iph, th);
        iph->check = RTE_IPV4_CKSUM(iph);
        rte_pktmbuf_data_len(m) = sizeof(struct eth_hdr)
                                + sizeof(struct iphdr)
                                + sizeof(struct tcphdr);
    } else {
        tcp_rst_set_ipv6((struct ip6_hdr *)iph);
        tcp_rst_set_tcp(th);
        th->th_sum = RTE_IPV6_UDPTCP_CKSUM(iph, th);
        rte_pktmbuf_data_len(m) = sizeof(struct eth_hdr)
                                + sizeof(struct ip6_hdr)
                                + sizeof(struct tcphdr);
    }

    net_stats_rst_tx();
    work_space_tx_send(ws, m);
}

static inline void tcp_do_retransmit(struct work_space *ws, struct socket *sk)
{
    uint8_t flags = 0;

    if (sk->snd_nxt == sk->snd_una) {
        sk->retrans = 0;
        return;
    }

    sk->retrans++;
    if (sk->retrans < RETRANSMIT_NUM_MAX) {
        flags = sk->flags;
        SOCKET_LOG_ENABLE(sk);
        SOCKET_LOG(sk, "retrans");
        tcp_reply(ws, sk, flags);
        if (flags & TH_SYN) {
            net_stats_syn_rt();
        } else if (flags & TH_PUSH) {
            net_stats_push_rt();
        } else if (flags & TH_FIN) {
            net_stats_fin_rt();
        } else {
            net_stats_ack_rt();
        }
    } else {
        SOCKET_LOG(sk, "err-socket");
        net_stats_socket_error();
        socket_close(sk);
    }
}

static inline void tcp_server_process_syn(struct work_space *ws, struct socket *sk, struct rte_mbuf *m, struct tcphdr *th)
{
    /* fast recycle */
    if (sk->state == SK_TIME_WAIT) {
        sk->state = SK_CLOSED;
    }

    if (sk->state == SK_CLOSED) {
        socket_server_open(&ws->socket_table, sk, th);
        tcp_reply(ws, sk, TH_SYN | TH_ACK);
    } else if (sk->state == SK_SYN_RECEIVED) {
        /* syn-ack lost, resend it */
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "syn-ack-lost");
        SOCKET_LOG(sk, "syn-ack-lost");
        if ((sk->timer_ticks + TICKS_PER_SEC) < work_space_ticks(ws)) {
            tcp_reply(ws, sk, TH_SYN | TH_ACK);
            net_stats_syn_rt();
        }
    } else {
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "drop-syn");
        SOCKET_LOG(sk, "drop-bad-syn");
        net_stats_tcp_drop();
    }

    mbuf_free2(m);
}

static inline void tcp_client_process_syn_ack(struct work_space *ws, struct socket *sk,
    struct rte_mbuf *m, struct tcphdr *th)
{
    uint32_t ack = ntohl(th->th_ack);
    uint32_t seq = htonl(th->th_seq);

    if (sk->state == SK_SYN_SENT) {
        if (ack != sk->snd_una) {
            SOCKET_LOG_ENABLE(sk);
            MBUF_LOG(m, "drop-syn-ack1");
            SOCKET_LOG(sk, "drop-syn-ack1");
            net_stats_tcp_drop();
            goto out;
        }
        sk->rcv_nxt = seq + 1;
        sk->snd_nxt = ack;
        sk->state = SK_ESTABLISHED;
        tcp_reply(ws, sk, TH_ACK | TH_PUSH);
    } else if (sk->state == SK_ESTABLISHED) {
        /* ack lost */
        net_stats_pkt_lost();
        if ((ack == sk->snd_una) && ((seq + 1) == sk->rcv_nxt)) {
            tcp_reply(ws, sk, TH_ACK);
            net_stats_ack_rt();
        }
    } else {
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "drop-syn-ack2");
        SOCKET_LOG(sk, "drop-syn-ack-bad2");
        net_stats_tcp_drop();
    }

out:
    mbuf_free2(m);
}

static inline bool tcp_check_sequence(struct work_space *ws, struct socket *sk, struct tcphdr *th, uint16_t data_len)
{
    uint32_t ack = ntohl(th->th_ack);
    uint32_t seq = ntohl(th->th_seq);
    uint32_t snd_nxt_old = sk->snd_nxt;

    if (th->th_flags & TH_FIN) {
        data_len++;
    }

    if ((ack == sk->snd_una) && (seq == sk->rcv_nxt)) {
        if (sk->state > SK_CLOSED) {
            /* skip syn  */
            sk->snd_nxt = ack;
            sk->rcv_nxt = seq + data_len;
            if (snd_nxt_old != ack) {
                socket_stop_retransmit_timer(sk);
            }
            return true;
        } else {
            return false;
        }
    }

    SOCKET_LOG_ENABLE(sk);
    /* my data packet lost */
    if (tcp_seq_le(ack, sk->snd_una)) {
        /* fast retransmit : If the last transmission time is more than 1 second */
        if ((sk->timer_ticks + TICKS_PER_SEC) < work_space_ticks(ws)) {
            tcp_do_retransmit(ws, sk);
        }
    } else if (ack == sk->snd_una) {
        /* lost ack */
        if (tcp_seq_le(seq, sk->rcv_nxt)) {
            tcp_reply(ws, sk, TH_ACK);
            net_stats_ack_rt();
        }
    }

    return false;
}

static inline uint8_t *tcp_data_get(struct iphdr *iph, struct tcphdr *th, uint16_t *data_len)
{
    struct ip6_hdr *ip6h = (struct ip6_hdr *)iph;
    uint16_t offset = 0;
    uint16_t len = 0;
    uint8_t *data = NULL;

    if (iph->version == 4) {
        offset = sizeof(struct iphdr) + th->th_off * 4;
        len = ntohs(iph->tot_len) - offset;
    } else {
        offset = sizeof(struct ip6_hdr) + th->th_off * 4;
        len = ntohs(ip6h->ip6_plen) - th->th_off * 4;
    }

    data = (uint8_t*)iph + offset;
    if (data_len) {
        *data_len = len;
    }

    return data;
}

static inline uint8_t tcp_process_fin(struct socket *sk, uint8_t rx_flags, uint8_t tx_flags)
{
    uint8_t flags = 0;

    switch (sk->state) {
        case SK_ESTABLISHED:
            if (rx_flags & TH_FIN) {
                sk->state = SK_LAST_ACK;
                flags = TH_FIN | TH_ACK;
            } else if (tx_flags & TH_FIN) {
                sk->state = SK_FIN_WAIT_1;
                flags = TH_FIN | TH_ACK;
            }
            break;
        case SK_FIN_WAIT_1:
            if (rx_flags & TH_FIN) {
                flags = TH_ACK;
                /* todo TIME WAIT */
                socket_close(sk);
            } else {
                sk->state = SK_FIN_WAIT_2;
            }
            break;
        case SK_LAST_ACK:
            socket_close(sk);
            break;
        case SK_FIN_WAIT_2:
            if (rx_flags & TH_FIN) {
                /* todo TIME WAIT */
                socket_close(sk);
            }
        case SK_TIME_WAIT:
        default:
            break;
    }

    return tx_flags | flags;
}

static inline void tcp_server_process_data(struct work_space *ws, struct socket *sk, struct rte_mbuf *m,
    struct iphdr *iph, struct tcphdr *th)
{
    uint8_t *data = NULL;
    uint8_t tx_flags = 0;
    uint8_t rx_flags = th->th_flags;
    uint16_t data_len = 0;

    data = tcp_data_get(iph, th,  &data_len);
    if (tcp_check_sequence(ws, sk, th, data_len) == false) {
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "drop-bad-seq");
        SOCKET_LOG(sk, "drop-bad-seq");
        net_stats_tcp_drop();
        goto out;
    }

    if (sk->state < SK_ESTABLISHED) {
        sk->state = SK_ESTABLISHED;
    }

    if (sk->state == SK_ESTABLISHED) {
        if (data_len) {
            http_parse_request(data, data_len);
            tx_flags |= TH_PUSH | TH_ACK;
            if (sk->keepalive == 0) {
                tx_flags |= TH_FIN;
            }
        }
    }

    if ((sk->state > SK_ESTABLISHED) || ((rx_flags | tx_flags) & TH_FIN)) {
        tx_flags = tcp_process_fin(sk, rx_flags, tx_flags);
    }

    if (tx_flags != 0) {
        tcp_reply(ws, sk, tx_flags);
    }

out:
    mbuf_free2(m);
}

static inline void tcp_client_process_data(struct work_space *ws, struct socket *sk, struct rte_mbuf *m,
    struct iphdr *iph, struct tcphdr *th)
{
    uint8_t *data = NULL;
    uint8_t tx_flags = 0;
    uint8_t rx_flags = th->th_flags;
    uint16_t data_len = 0;

    data = tcp_data_get(iph, th, &data_len);
    if (tcp_check_sequence(ws, sk, th, data_len) == false) {
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "drop-bad-seq");
        SOCKET_LOG(sk, "drop-bad-seq");
        net_stats_tcp_drop();
        goto out;
    }

    if (sk->state == SK_ESTABLISHED) {
        if (data_len) {
            http_parse_response(data, data_len);
            tx_flags |= TH_ACK;
            if (sk->keepalive == 0) {
                tx_flags |= TH_FIN;
            } else {
                socket_start_keepalive_timer(sk, work_space_ticks(ws));
            }
        }
    }

    if ((sk->state > SK_ESTABLISHED) || ((rx_flags | tx_flags) & TH_FIN)) {
        tx_flags = tcp_process_fin(sk, rx_flags, tx_flags);
    }

    if (tx_flags != 0) {
        /* delay ack */
        if ((rx_flags & TH_FIN) || (tx_flags != TH_ACK) || (sk->keepalive == 0)
            || (sk->keepalive && (g_config.keepalive_request_interval >= RETRANSMIT_TIMEOUT))) {
            tcp_reply(ws, sk, tx_flags);
        }
    }

out:
    mbuf_free2(m);
}

static inline void tcp_server_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct iphdr *iph = mbuf_ip_hdr(m);
    struct tcphdr *th = mbuf_tcp_hdr(m);
    uint8_t flags = th->th_flags;
    struct socket *sk = NULL;

    sk = socket_server_lookup(&ws->socket_table, iph, th);
    if (unlikely(sk == NULL)) {
        if (ws->kni) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop-no-socket");
        tcp_reply_rst(ws, m);
        return;
    }

#ifdef DPERF_DEBUG
    if ((flags & TH_SYN) && ((th->th_dport) == htons(80))) {
        MBUF_LOG(m, "syn-rx");
        SOCKET_LOG(sk, "syn-rx");
    } else if ((sk->log) && ((th->th_dport) == htons(80))) {
        SOCKET_LOG(sk, "rx");
        MBUF_LOG(m, "rx");
    }
#endif

    tcp_flags_rx_count(flags);
    if (((flags & (TH_SYN | TH_RST)) == 0) && (flags & TH_ACK)) {
        return tcp_server_process_data(ws, sk, m, iph, th);
    } else if (flags == TH_SYN) {
        return tcp_server_process_syn(ws, sk, m, th);
    } else if (flags & TH_RST) {
        return tcp_process_rst(sk, m);
    } else {
        MBUF_LOG(m, "drop-bad-flags");
        SOCKET_LOG(sk, "drop-bad-flags");
        net_stats_tcp_drop();
        mbuf_free2(m);
    }
}

static inline void tcp_client_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct iphdr *iph = mbuf_ip_hdr(m);
    struct tcphdr *th = mbuf_tcp_hdr(m);
    uint8_t flags = th->th_flags;
    struct socket *sk = NULL;

    sk = socket_client_lookup(&ws->socket_table, iph, th);
    if (unlikely(sk == NULL)) {
        if (ws->kni) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop-no-socket");
        tcp_reply_rst(ws, m);
        return;
    }

#ifdef DPERF_DEBUG
    if ((flags & TH_SYN) && ((th->th_sport) == htons(80))) {
        MBUF_LOG(m, "syn-rx");
        SOCKET_LOG(sk, "syn-rx");
    } else if ((sk->log) && ((th->th_sport) == htons(80))) {
        SOCKET_LOG(sk, "rx");
        MBUF_LOG(m, "rx");
    }
#endif

    tcp_flags_rx_count(flags);
    if (((flags & (TH_SYN | TH_RST)) == 0) && (flags & TH_ACK)) {
        return tcp_client_process_data(ws, sk, m, iph, th);
    } else if (flags == (TH_SYN | TH_ACK)) {
        return tcp_client_process_syn_ack(ws, sk, m, th);
    } else if (flags & TH_RST) {
        return tcp_process_rst(sk, m);
    } else {
        SOCKET_LOG_ENABLE(sk);
        MBUF_LOG(m, "drop-bad-flags");
        SOCKET_LOG(sk, "drop-bad-flags");
        net_stats_tcp_drop();
        mbuf_free2(m);
    }
}

static inline void tcp_do_keepalive(struct work_space *ws, struct socket *sk)
{
    if ((sk->snd_nxt != sk->snd_una) || (sk->state != SK_ESTABLISHED) || (sk->keepalive == 0)) {
        return;
    }

    if (g_config.server == 0) {
        if (work_space_in_duration(ws)) {
            sk->keepalive_request_num++;
            tcp_reply(ws, sk, TH_ACK | TH_PUSH);
            if (unlikely((g_config.keepalive_request_num > 0)
                         && (sk->keepalive_request_num >= g_config.keepalive_request_num))) {
                sk->keepalive = 0;
            }
        } else {
            sk->state = SK_FIN_WAIT_1;
            tcp_reply(ws, sk, TH_ACK | TH_FIN);
            sk->keepalive = 0;
        }
    }
}

static inline int tcp_client_launch(struct work_space *ws)
{
    int synflood = 0;
    uint64_t i = 0;
    uint64_t num = 0;
    struct socket *sk = NULL;

    synflood = ws->cfg->synflood;
    num = work_space_client_launch_num(ws);
    for (i = 0; i < num; i++) {
        sk = socket_client_open(&ws->socket_table);
        if (unlikely(sk == NULL)) {
            continue;
        }

        tcp_reply(ws, sk, TH_SYN);
        if (synflood) {
            socket_close(sk);
        }
    }

    return 0;
}

static int tcp_client_socket_timer_process(struct work_space *ws)
{
    struct socket_timer *rt_timer = &g_retransmit_timer;
    struct socket_timer *kp_timer = &g_keepalive_timer;

    socket_timer_run(ws, rt_timer, RETRANSMIT_TIMEOUT, tcp_do_retransmit);
    if (g_config.keepalive) {
        socket_timer_run(ws, kp_timer, g_config.keepalive_request_interval, tcp_do_keepalive);
    }

    return 0;
}

static inline int tcp_server_socket_timer_process(struct work_space *ws)
{
    struct socket_timer *rt_timer = &g_retransmit_timer;

    /* server delays sending by 0.1s to avoid simultaneous retransmission */
    socket_timer_run(ws, rt_timer, RETRANSMIT_TIMEOUT + (RETRANSMIT_TIMEOUT / 10), tcp_do_retransmit);
    return 0;
}

static void tcp_server_run_loop_ipv4(struct work_space *ws)
{
    server_loop(ws, ipv4_input, tcp_server_process, udp_drop, tcp_server_socket_timer_process);
}

static void tcp_server_run_loop_ipv6(struct work_space *ws)
{
    server_loop(ws, ipv6_input, tcp_server_process, udp_drop, tcp_server_socket_timer_process);
}

static void tcp_client_run_loop_ipv4(struct work_space *ws)
{
    client_loop(ws, ipv4_input, tcp_client_process, udp_drop, tcp_client_socket_timer_process, tcp_client_launch);
}

static void tcp_client_run_loop_ipv6(struct work_space *ws)
{
    client_loop(ws, ipv6_input, tcp_client_process, udp_drop, tcp_client_socket_timer_process, tcp_client_launch);
}

static void tcp_server_run_loop_vxlan(struct work_space *ws)
{
    server_loop(ws, vxlan_input, tcp_server_process, udp_drop, tcp_server_socket_timer_process);
}

static void tcp_client_run_loop_vxlan(struct work_space *ws)
{
    client_loop(ws, vxlan_input, tcp_client_process, udp_drop, tcp_client_socket_timer_process, tcp_client_launch);
}

int tcp_init(struct work_space *ws)
{
    const char *data = NULL;

    if (g_config.protocol != IPPROTO_TCP) {
        return 0;
    }

    if (g_config.server) {
        data = http_get_response();
        if (ws->vxlan) {
            ws->run_loop = tcp_server_run_loop_vxlan;
        } else if (ws->ipv6) {
            ws->run_loop = tcp_server_run_loop_ipv6;
        } else {
            ws->run_loop = tcp_server_run_loop_ipv4;
        }
    } else {
        data = http_get_request();
        if (ws->vxlan) {
            ws->run_loop = tcp_client_run_loop_vxlan;
        } else if (ws->ipv6) {
            ws->run_loop = tcp_client_run_loop_ipv6;
        } else {
            ws->run_loop = tcp_client_run_loop_ipv4;
        }
    }

    if (mbuf_cache_init_tcp(&ws->tcp, ws, "tcp", 0, NULL) < 0) {
        return -1;
    }

    if (mbuf_cache_init_tcp(&ws->tcp_opt, ws, "tcp_opt", ws->cfg->mss, NULL) < 0) {
        return -1;
    }

    if (mbuf_cache_init_tcp(&ws->tcp_data, ws, "tcp_data", 0, data) < 0) {
        return -1;
    }

    return 0;
}

void tcp_drop(__rte_unused struct work_space *ws, struct rte_mbuf *m)
{
    if (m) {
        if (ws->kni) {
            return kni_recv(ws, m);
        }
        net_stats_udp_drop();
        mbuf_free(m);
    }
}

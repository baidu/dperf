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
#include "http_parse.h"

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

static inline void tcp_change_dipv6(struct work_space *ws, struct ip6_hdr *ip6h, struct tcphdr *th)
{
    ipaddr_t addr_old;
    ipaddr_t addr_new;
    struct ip_list *ip_list = NULL;

    ip_list = &ws->dip_list;
    addr_old.in6 = ip6h->ip6_dst;
    ip_list_get_next_ipv6(ip_list, &addr_new.in6);
    ip6h->ip6_dst = addr_new.in6;
    th->th_sum = csum_update_u128(th->th_sum, (uint32_t *)&addr_old, (uint32_t *)&addr_new);
}

static inline void tcp_change_dipv4(struct work_space *ws, struct iphdr *iph, struct tcphdr *th)
{
    uint32_t addr_old = 0;
    uint32_t addr_new = 0;
    struct ip_list *ip_list = NULL;

    ip_list = &ws->dip_list;
    addr_old = iph->daddr;
    ip_list_get_next_ipv4(ip_list, &addr_new);
    iph->daddr = addr_new;
    iph->check = csum_update_u32(iph->check, addr_old, addr_new);
    th->th_sum = csum_update_u32(th->th_sum, addr_old, addr_new);
}

static inline void tcp_change_dip(struct work_space *ws, struct iphdr *iph, struct tcphdr *th)
{
    if (ws->ipv6) {
        tcp_change_dipv6(ws, (struct ip6_hdr *)iph, th);
    } else {
        tcp_change_dipv4(ws, iph, th);
    }
}

static inline struct rte_mbuf *tcp_new_packet(struct work_space *ws, struct socket *sk, uint8_t tcp_flags)
{
    uint16_t csum_ip = 0;
    uint16_t csum_tcp = 0;
    uint16_t snd_seq = 0;
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
        snd_seq = p->data.data_len;
        if (tcp_flags & TH_URG) {
            tcp_flags &= ~(TH_URG|TH_PUSH);
        }
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
        snd_seq++;
    }

    if (sk->snd_nxt == sk->snd_una) {
        sk->snd_nxt += snd_seq;
    }

    /* update vxlan inner header */
    if (ws->vxlan) {
        iph = (struct iphdr *)((uint8_t *)mbuf_eth_hdr(m) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
        if (ws->ipv6) {
            ip6h = (struct ip6_hdr *)iph;
            th = (struct tcphdr *)((uint8_t *)ip6h + sizeof(struct ip6_hdr));
        } else {
            th = (struct tcphdr *)((uint8_t *)iph + sizeof(struct iphdr));
            iph->check = csum_update_u16(csum_ip, 0, htons(ws->ip_id));
        }

        csum_tcp = csum_update_u32(csum_tcp, htonl(sk->snd_nxt), htonl(sk->rcv_nxt));
        csum_tcp = csum_update_u16(csum_tcp, 0, htons(tcp_flags));
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
    /* we always send from <snd_una> */
    th->th_seq = htonl(sk->snd_una);
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

    /* only in client mode */
    if (ws->change_dip) {
        tcp_change_dip(ws, iph, th);
    }

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
    uint64_t now_tsc = 0;

    now_tsc = work_space_tsc(ws);
    sk->flags = tcp_flags;
    tcp_flags_tx_count(tcp_flags);

    if (tcp_flags & TH_PUSH) {
        if (g_config.server == 0) {
            net_stats_tcp_req();
            if (g_config.http_method == HTTP_METH_GET) {
                net_stats_http_get();
            } else {
                net_stats_http_post();
            }
        } else {
            if (ws->send_window == 0) {
                net_stats_tcp_rsp();
                net_stats_http_2xx();
            }
        }
    }

    m = tcp_new_packet(ws, sk, tcp_flags);
    if (m) {
        work_space_tx_send_tcp(ws, m);
    }

    if (sk->state != SK_CLOSED) {
        if (tcp_flags & (TH_PUSH | TH_SYN | TH_FIN)) {
            /* for accurate PPS */
            if ((!ws->server) && (sk->keepalive != 0)) {
                now_tsc = socket_accurate_timer_tsc(sk, now_tsc);
            }
            if (ws->send_window == 0) {
                socket_start_retransmit_timer(sk, now_tsc);
            }
        } else if (ws->server && (ws->send_window == 0)) {
            socket_start_timeout_timer(sk, now_tsc);
        }
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
    int nlen = 0;
    int olen = 0;

    /* not support rst yet */
    if (ws->vxlan) {
        net_stats_tcp_drop();
        mbuf_free2(m);
        return;
    }

    eth = mbuf_eth_hdr(m);
    iph = mbuf_ip_hdr(m);
    th = mbuf_tcp_hdr(m);
    olen = rte_pktmbuf_data_len(m);
    eth_addr_swap(eth);
    if (iph->version == 4) {
        tcp_rst_set_ip(iph);
        tcp_rst_set_tcp(th);
        th->th_sum = RTE_IPV4_UDPTCP_CKSUM(iph, th);
        iph->check = RTE_IPV4_CKSUM(iph);
        nlen = sizeof(struct eth_hdr) + sizeof(struct iphdr) + sizeof(struct tcphdr);
    } else {
        tcp_rst_set_ipv6((struct ip6_hdr *)iph);
        tcp_rst_set_tcp(th);
        th->th_sum = RTE_IPV6_UDPTCP_CKSUM(iph, th);
        nlen = sizeof(struct eth_hdr) + sizeof(struct ip6_hdr) + sizeof(struct tcphdr);
    }

    if (olen > nlen) {
        rte_pktmbuf_trim(m, olen - nlen);
    }
    net_stats_rst_tx();
    work_space_tx_send(ws, m);
}

static inline void tcp_do_retransmit(struct work_space *ws, struct socket *sk)
{
    uint32_t snd_nxt = 0;
    uint8_t flags = 0;

    if (sk->snd_nxt == sk->snd_una) {
        sk->retrans = 0;
        return;
    }

    /* rss auto: this socket is closed by another worker */
    if (unlikely(sk->laddr == 0)) {
        socket_close(sk);
        return;
    }

    sk->retrans++;
    if (sk->retrans < RETRANSMIT_NUM_MAX) {
        flags = sk->flags;
        SOCKET_LOG_ENABLE(sk);
        SOCKET_LOG(sk, "retrans");
        if ((ws->send_window) && (sk->snd_nxt != sk->snd_una) && (flags & TH_PUSH)) {
            snd_nxt = sk->snd_nxt;
            sk->snd_nxt = sk->snd_una;
            tcp_reply(ws, sk, TH_PUSH | TH_ACK);
            sk->snd_nxt = snd_nxt;
            sk->snd_window = 1;
            net_stats_push_rt();
            socket_start_retransmit_timer(sk, work_space_tsc(ws));
            return;
        } else {
            tcp_reply(ws, sk, flags);
        }
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
        if ((sk->timer_tsc + TSC_PER_SEC) < work_space_tsc(ws)) {
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
        if (ack != sk->snd_nxt) {
            SOCKET_LOG_ENABLE(sk);
            MBUF_LOG(m, "drop-syn-ack1");
            SOCKET_LOG(sk, "drop-syn-ack1");
            net_stats_tcp_drop();
            goto out;
        }

        net_stats_rtt(ws, sk);
        sk->rcv_nxt = seq + 1;
        sk->snd_una = ack;
        sk->state = SK_ESTABLISHED;
        tcp_reply(ws, sk, TH_ACK | TH_PUSH);
    } else if (sk->state == SK_ESTABLISHED) {
        /* ack lost */
        net_stats_pkt_lost();
        if ((ack == sk->snd_nxt) && ((seq + 1) == sk->rcv_nxt)) {
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
    uint32_t snd_last = sk->snd_una;
    uint32_t snd_nxt = 0;

    if (th->th_flags & TH_FIN) {
        data_len++;
    }

    if ((ack == sk->snd_nxt) && (seq == sk->rcv_nxt)) {
        sk->retrans = 0;
        if (sk->state > SK_CLOSED) {
            /* skip syn  */
            sk->snd_una = ack;
            sk->rcv_nxt = seq + data_len;

            if (ws->send_window == 0) {
                if (snd_last != ack) {
                    socket_stop_retransmit_timer(sk);
                }
            } else {
                if (sk->snd_window < SEND_WINDOW_MAX) {
                    sk->snd_window++;
                }
            }
            return true;
        } else {
            return false;
        }
    }

    /* CLOSING */
    if ((sk->state == SK_FIN_WAIT_1) && (th->th_flags & TH_FIN) && (data_len == 1) &&
        (ack == snd_last) && (seq == sk->rcv_nxt)) {
        sk->rcv_nxt = seq + 1;
        sk->state = SK_CLOSING;
        return true;
    }

    /* stale packet */
    if (unlikely(sk->state == SK_CLOSED)) {
        return false;
    }

    SOCKET_LOG_ENABLE(sk);
    /* my data packet lost */
    if (tcp_seq_le(ack, sk->snd_nxt)) {
        if (ws->send_window) {
            /* new data is acked */
            if ((tcp_seq_gt(ack, sk->snd_una))) {
                sk->snd_una = ack;
                if (sk->snd_window < SEND_WINDOW_MAX) {
                    sk->snd_window++;
                }
                sk->retrans = 0;
                return true;
            } else if (ack == sk->snd_una) {
                sk->retrans++;
                net_stats_ack_dup();
                /* 3 ACK means packets loss. */
                if (sk->retrans < 3) {
                    return false;
                }

                snd_nxt = sk->snd_nxt;
                sk->snd_nxt = ack;
                tcp_reply(ws, sk, TH_PUSH | TH_ACK);
                sk->snd_nxt = snd_nxt;
                sk->retrans = 0;
                sk->snd_window = 1;
                return false;
            } else {
                /* stale ack */
                return false;
            }
        } else {
            /* fast retransmit : If the last transmission time is more than 1 second */
            if ((sk->timer_tsc + TSC_PER_SEC) < work_space_tsc(ws)) {
                tcp_do_retransmit(ws, sk);
            }
        }
    } else if (ack == sk->snd_nxt) {
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
                /* enter TIME WAIT */
                socket_close(sk);
            } else {
                /* wait FIN */
                sk->state = SK_FIN_WAIT_2;
            }
            break;
        case SK_CLOSING:
            /* In order to prevent the loss of fin, we make up a FIN, which has no cost */
            sk->state = SK_LAST_ACK;
            flags = TH_FIN | TH_ACK;
            break;
        case SK_LAST_ACK:
            socket_close(sk);
            break;
        case SK_FIN_WAIT_2:
            /* FIN is here */
            if (rx_flags & TH_FIN) {
                flags = TH_ACK;
                /* enter TIME WAIT */
                socket_close(sk);
            }
        case SK_TIME_WAIT:
        default:
            break;
    }

    return tx_flags | flags;
}

static inline void tcp_reply_more(struct work_space *ws, struct socket *sk)
{
    int i = 0;
    uint32_t snd_una = sk->snd_una;
    uint32_t snd_max = sk->snd_max;
    uint32_t snd_wnd = snd_una + ws->send_window;

    /* wait a burst finish */
    while (tcp_seq_lt(sk->snd_nxt, snd_wnd) && tcp_seq_lt(sk->snd_nxt, snd_max) && (i < sk->snd_window)) {
        sk->snd_una = sk->snd_nxt;
        tcp_reply(ws, sk, TH_PUSH | TH_ACK | TH_URG);
        i++;
    }

    sk->snd_una = snd_una;
    if (snd_una == snd_max) {
        if (sk->keepalive) {
            socket_start_timeout_timer(sk,  work_space_tsc(ws));
        } else {
            sk->state = SK_FIN_WAIT_1;
            tcp_reply(ws, sk, TH_FIN | TH_ACK);
        }
    }
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
            if ((ws->send_window) && ((rx_flags & TH_FIN) == 0)) {
                socket_init_http_server(sk);
                net_stats_tcp_rsp();
                net_stats_http_2xx();
                if (sk->keepalive_request_num) {
                    tcp_reply_more(ws, sk);
                } else {
                    /* slow start */
                    tcp_reply(ws, sk, TH_PUSH | TH_ACK);
                    sk->keepalive_request_num = 1;
                }
                socket_start_retransmit_timer(sk, work_space_tsc(ws));
                goto out;
            } else {
                tx_flags |= TH_PUSH | TH_ACK;
                if (sk->keepalive == 0) {
                    tx_flags |= TH_FIN;
                }
            }
        } else if ((ws->send_window) && ((rx_flags & TH_FIN) == 0)) {
            tcp_reply_more(ws, sk);
            goto out;
        }
    }

    if ((sk->state > SK_ESTABLISHED) || ((rx_flags | tx_flags) & TH_FIN)) {
        tx_flags = tcp_process_fin(sk, rx_flags, tx_flags);
    }

    if (tx_flags != 0) {
        tcp_reply(ws, sk, tx_flags);
    } else if (sk->state != SK_CLOSED) {
        socket_start_timeout_timer(sk, work_space_tsc(ws));
    }

out:
    mbuf_free2(m);
}

#ifdef HTTP_PARSE
static inline void tcp_ack_delay_add(struct work_space *ws, struct socket *sk)
{
    if (sk->http_ack) {
        return;
    }

    if (ws->ack_delay.next >= TCP_ACK_DELAY_MAX) {
        tcp_ack_delay_flush(ws);
    }

    ws->ack_delay.sockets[ws->ack_delay.next] = sk;
    ws->ack_delay.next++;
    sk->http_ack = 1;
}

static inline uint8_t http_client_process_data(struct work_space *ws, struct socket *sk,
    uint8_t rx_flags, uint8_t *data, uint16_t data_len)
{
    int ret = 0;
    int8_t tx_flags = 0;
    uint8_t http_frags = 0;

    ret = http_parse_run(sk, data, data_len);
    if (ret == HTTP_PARSE_OK) {
        if (sk->http_frags < 4) {
            sk->http_frags++;
        }
        if ((rx_flags & TH_FIN) == 0) {
            tcp_ack_delay_add(ws, sk);
            return 0;
        }
    } else if (ret == HTTP_PARSE_END) {
        http_frags = sk->http_frags;
        socket_init_http(sk);
        if (sk->keepalive && ((rx_flags & TH_FIN) == 0)) {
            /* we should ack now:
             * 1. 3 http fragments are not ACKed
             * 2. we want ack each data quickly(disable_ack == 0), except we will send out our next request shortly.
             * */
            if ((http_frags >= 2) || ((g_config.keepalive_request_interval >= g_config.retransmit_timeout) && (ws->disable_ack == 0))) {
                tcp_ack_delay_add(ws, sk);
            }
            socket_start_keepalive_timer(sk, work_space_tsc(ws));
            return 0;
        } else {
            tx_flags |= TH_FIN;
            sk->http_ack = 0;
        }
    } else {
        socket_init_http(sk);
        sk->keepalive = 0;
        sk->http_length = 0;
        tx_flags |= TH_FIN;
        net_stats_http_error();
    }

    return TH_ACK | tx_flags;
}
#endif

static inline void tcp_send_keepalive_request(struct work_space *ws, struct socket *sk)
{
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
#ifdef HTTP_PARSE
            if (ws->http) {
                tx_flags = http_client_process_data(ws, sk, rx_flags, data, data_len);
            } else
#endif
            {
                tx_flags |= TH_ACK;
                http_parse_response(data, data_len);
                if (sk->keepalive == 0) {
                    tx_flags |= TH_FIN;
                } else {
                    if (g_config.keepalive_request_interval) {
                        socket_start_keepalive_timer(sk, sk->timer_tsc);
                    } else {
                        tcp_send_keepalive_request(ws, sk);
                    }
                }
            }
        }

        if ((tx_flags & TH_FIN) && ws->fast_close) {
            tcp_reply(ws, sk, TH_RST | TH_ACK);
            socket_close(sk);
            goto out;
        }
    }

    if ((sk->state > SK_ESTABLISHED) || ((rx_flags | tx_flags) & TH_FIN)) {
        tx_flags = tcp_process_fin(sk, rx_flags, tx_flags);
    }

    if (tx_flags != 0) {
        /* delay ack */
        if ((rx_flags & TH_FIN) || (tx_flags != TH_ACK) || (sk->keepalive == 0)
            || (sk->keepalive && (g_config.keepalive_request_interval >= g_config.retransmit_timeout) && (ws->disable_ack == 0))) {
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
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop-no-socket");
        if (g_config.tcp_rst) {
            tcp_reply_rst(ws, m);
        } else {
            net_stats_tcp_drop();
            mbuf_free2(m);
        }
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
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        MBUF_LOG(m, "drop-no-socket");
        if (g_config.tcp_rst) {
            tcp_reply_rst(ws, m);
        } else {
            net_stats_tcp_drop();
            mbuf_free2(m);
        }
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
        if (ws->flood) {
            if (work_space_in_duration(ws)) {
                tcp_reply(ws, sk, TH_SYN);
                sk->snd_una = sk->snd_nxt;
                socket_start_keepalive_timer(sk, work_space_tsc(ws));
            } else {
                socket_close(sk);
            }
        }
        return;
    }

    if (g_config.server == 0) {
        tcp_send_keepalive_request(ws, sk);
    }
}

static inline int tcp_client_launch(struct work_space *ws)
{
    uint64_t i = 0;
    uint64_t num = 0;
    struct socket *sk = NULL;

    num = work_space_client_launch_num(ws);
    for (i = 0; i < num; i++) {
        sk = socket_client_open(&ws->socket_table, work_space_tsc(ws));
        if (unlikely(sk == NULL)) {
            continue;
        }

        tcp_reply(ws, sk, TH_SYN);
        if (ws->flood) {
            if (sk->keepalive) {
                sk->snd_una = sk->snd_nxt;
                socket_start_keepalive_timer(sk, work_space_tsc(ws));
            } else {
                socket_close(sk);
            }
        }
    }

    return num;
}

static int tcp_client_socket_timer_process(struct work_space *ws)
{
    struct socket_timer *rt_timer = &g_retransmit_timer;
    struct socket_timer *kp_timer = &g_keepalive_timer;

    socket_timer_run(ws, rt_timer, g_config.retransmit_timeout, tcp_do_retransmit);
    if (g_config.keepalive) {
        socket_timer_run(ws, kp_timer, g_config.keepalive_request_interval, tcp_do_keepalive);
    }

    return 0;
}

static inline int tcp_server_socket_timer_process(struct work_space *ws)
{
    uint64_t timeout = 0;
    struct socket_timer *rt_timer = &g_retransmit_timer;

    timeout = g_config.retransmit_timeout + (g_config.retransmit_timeout / 10);
    /* server delays sending by 0.1s to avoid simultaneous retransmission */
    socket_timer_run(ws, rt_timer, timeout, tcp_do_retransmit);
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
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        net_stats_tcp_drop();
        mbuf_free(m);
    }
}

#ifdef HTTP_PARSE
int tcp_ack_delay_flush(struct work_space *ws)
{
    int i = 0;
    struct socket *sk = NULL;

    for (i = 0; i < ws->ack_delay.next; i++) {
        sk = ws->ack_delay.sockets[i];
        if (sk->http_ack) {
            sk->http_ack = 0;
            if (sk->state == SK_ESTABLISHED) {
                tcp_reply(ws, sk, TH_ACK);
            }
       }
    }

    if (ws->ack_delay.next > 0) {
        ws->ack_delay.next = 0;
    }

    return 0;
}
#endif

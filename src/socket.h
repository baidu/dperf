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

#ifndef __SOCKET_H
#define __SOCKET_H

#include "config.h"
#include "tcp.h"
#include "ip.h"
#include "tick.h"
#include "net_stats.h"

enum {
    SK_CLOSED,
    SK_LISTEN,
    SK_SYN_SENT,        /* also UDP client send */
    SK_SYN_RECEIVED,    /* also UDP client recv */
    SK_ESTABLISHED,
    SK_CLOSE_WAIT, /* unused */
    SK_FIN_WAIT_1,
    SK_CLOSING,
    SK_LAST_ACK,
    SK_FIN_WAIT_2,
    SK_TIME_WAIT
};

extern const char *g_sk_states[];

#define RETRANSMIT_NUM_MAX          4
#define REQUEST_INTERVAL_DEFAULT    (TSC_PER_SEC * 60)

struct socket_node {
    struct socket_node *next;
    struct socket_node *prev;
};

/* 64 bytes in a cache line */
struct socket {
    /* ------16 bytes------  */
    struct socket_node node;    /* must be first */

    /* ------16 bytes------  */
    uint32_t rcv_nxt;
    uint32_t snd_nxt;
    uint32_t snd_una;

    uint8_t flags; /* tcp flags*/
    uint8_t state:4;
    uint8_t retrans:3;
    uint8_t keepalive:1;
    uint16_t log:1;
    uint16_t keepalive_request_num:15;

    /* ------16 bytes------  */
    uint64_t timer_tsc;
    uint16_t csum_tcp_data;
    uint16_t csum_ip;
    uint16_t csum_ip_opt;
    uint16_t csum_ip_data;

    /* ------16 bytes------  */
    uint32_t laddr;
    uint32_t faddr;
    uint16_t lport;
    uint16_t fport;
    union {
        uint16_t csum_tcp;
        uint16_t csum_udp;
    };
    uint16_t csum_tcp_opt;

#ifdef HTTP_PARSE
    /* ------16 bytes------  */
    /* http protocol         */
    int64_t http_length;
    uint8_t http_parse_state;
    uint8_t http_flags;
    uint8_t http_ack:1;
    uint8_t http_frags:7;
    uint8_t snd_window;
    uint32_t snd_max;
#endif
};

struct socket_port_table {
    struct socket sockets[0];
};

struct socket_pool {
    uint32_t num;
    uint32_t next;
    struct socket base[0] __attribute__((__aligned__(CACHE_ALIGN_SIZE)));
};

struct socket_table {
    uint32_t server_ip;
    uint16_t server_port_num;
    uint16_t server_port_min;
    uint16_t server_port_max;

    uint16_t client_port_num;
    uint16_t client_port_min;
    uint16_t client_port_max;

    uint8_t client_hop;
    uint8_t rss;
    uint8_t rss_id;
    uint8_t rss_num;
    struct socket_table *socket_table_hash[256]; /* server rss hash */
                     /*[client-ip][client-port][server-port]*/
    struct socket_port_table *ht[NETWORK_PORT_NUM];
    struct socket_pool socket_pool;
};

static inline void socket_node_init(struct socket_node *sn)
{
    sn->next = sn;
    sn->prev = sn;
}

static inline void socket_node_del(struct socket_node *sn)
{
    struct socket_node *prev = sn->prev;
    struct socket_node *next = sn->next;

    if (sn != next) {
        prev->next = next;
        next->prev = prev;
        socket_node_init(sn);
    }
}

static inline struct socket *socket_table_get_socket_rss(struct socket_table *st)
{
    struct socket *sk = NULL;
    struct socket_pool *sp = &st->socket_pool;

    while (1) {
        sk = &(sp->base[sp->next]);
        if (st->rss == RSS_AUTO) {
            sp->next++;
            /* 1. each worker picks sockets in port order
             * 2. avoid incorrectly hashed sockets
             * */
            if (((ntohs(sk->lport) % st->rss_num) != st->rss_id) || (sk->laddr == 0)) {
                if (sp->next >= sp->num) {
                    sp->next = 0;
                }
                continue;
            }
            break;
        }

        if (sk->laddr != 0) {
            sp->next++;
            if (sp->next >= sp->num) {
                sp->next = 0;
            }
            break;
        } else if (st->rss == RSS_L3L4) {
            sp->next++;
            if (sp->next >= sp->num) {
                sp->next = 0;
            }
            continue;
        } else {
            sp->next += st->client_port_num * st->server_port_num;
            if (sp->next >= sp->num) {
                sp->next = 0;
            }
            continue;
        }
    }

    return sk;
}

static inline struct socket *socket_table_get_socket(struct socket_table *st)
{
    struct socket *sk = NULL;
    struct socket_pool *sp = &st->socket_pool;

retry:
    if (st->rss == RSS_NONE) {
        sk = &(sp->base[sp->next]);
        sp->next++;
        if (sp->next >= sp->num) {
            sp->next = 0;
        }
    } else {
        sk = socket_table_get_socket_rss(st);
    }

    if (st->client_hop) {
        sp->next += 65535;
        if (sp->next >= sp->num) {
            sp->next = 0;
        }
    }

    return sk;
}

static inline struct socket *socket_port_table_get(const struct socket_table *st,
    struct socket_port_table *t, uint16_t client_port_host, uint16_t server_port_host)
{
    uint32_t idx = 0;

    idx = (client_port_host - st->client_port_min) * st->server_port_num + server_port_host - st->server_port_min;
    return &t->sockets[idx];
}

static inline struct socket *socket_common_lookup(const struct socket_table *st, uint32_t client_ip, uint32_t server_ip
    , uint16_t client_port, uint16_t server_port)
{
    uint32_t client_low_2byte = ntohl(client_ip) & 0xffff;
    struct socket_port_table *t = st->ht[client_low_2byte];
    uint16_t client_port_host = ntohs(client_port);
    uint16_t server_port_host = ntohs(server_port);

    if (likely(t != NULL)  && (server_ip == st->server_ip) && (client_port != 0) &&
        (client_port_host >= st->client_port_min) && (client_port_host <= st->client_port_max) &&
        (server_port_host >= st->server_port_min) && (server_port_host <= st->server_port_max)) {
        return socket_port_table_get(st, t, client_port_host, server_port_host);
    }

    return NULL;
}

static inline void socket_dup(struct socket *dst, struct socket *src)
{
    /*
     * TCP: we cannot break a tcp connection, so we keep this socket alive here.
     * */
    if (src->flags) {
        dst->rcv_nxt = src->rcv_nxt;
        dst->snd_nxt = src->snd_nxt;
        dst->snd_una = src->snd_una;
        dst->flags = src->flags;
        dst->keepalive = src->keepalive;
    } else {
        /* UDP: close keepalive, just count */
        dst->keepalive = 0;
    }
    dst->timer_tsc = src->timer_tsc;
    dst->state = src->state;
    net_stats_socket_dup();
}

static inline void socket_client_check_rss_auto(const struct socket_table *st,
    uint32_t daddr, uint32_t saddr, uint16_t dport, uint16_t sport, struct socket *sk)
{
    uint32_t idx = 0;
    struct socket *sk2 = NULL;
    const struct socket_table *st2 = NULL;

    idx = ntohs(dport) % st->rss_num;
    if (idx != st->rss_id) {
        st2 = st->socket_table_hash[idx];
        if (unlikely(st2 == NULL)) {
            return;
        }

        sk2 = socket_common_lookup(st2, daddr, saddr, dport, sport);
        if (sk2 && (sk2->laddr == daddr) && (sk2->faddr == saddr)) {
            socket_dup(sk, sk2);
            sk2->laddr = 0;
            sk2->faddr = 0;
        }
    }
}

static inline struct socket *socket_client_lookup(const struct socket_table *st, const struct iphdr *iph,
    const struct tcphdr *th)
{
    uint32_t saddr = 0;
    uint32_t daddr = 0;
    struct socket *sk = NULL;

    ip_hdr_get_addr_low32(iph, saddr, daddr);
    sk = socket_common_lookup(st, daddr, saddr, th->th_dport, th->th_sport);
    if (sk && (sk->laddr == daddr) && (sk->faddr == saddr)) {
        if (unlikely(st->rss == RSS_AUTO)) {
            socket_client_check_rss_auto(st, daddr, saddr, th->th_dport, th->th_sport, sk);
        }
        return sk;
    }

    return NULL;
}

static inline struct socket *socket_server_lookup(const struct socket_table *st, const struct iphdr *iph,
    const struct tcphdr *th)
{
    uint32_t idx = 0;
    uint32_t saddr = 0;
    uint32_t daddr = 0;
    struct socket *sk = NULL;

    ip_hdr_get_addr_low32(iph, saddr, daddr);

    if (unlikely(st->rss == RSS_L3)) {
        idx = ntohl(daddr) & 0xff;
        st = st->socket_table_hash[idx];
        if (unlikely(st == NULL)) {
            return NULL;
        }
    }

    sk = socket_common_lookup(st, saddr, daddr, th->th_sport, th->th_dport);
    if (sk && (sk->laddr == daddr) && (sk->faddr == saddr)) {
        return sk;
    }

    return NULL;
}

static inline void socket_close(struct socket *sk)
{
    if (sk->state != SK_CLOSED) {
        sk->state = SK_CLOSED;
        /* don't clear sequences in TIME-WAIT */
        socket_node_del(&sk->node);
        net_stats_socket_close();
    }
}

#ifdef HTTP_PARSE
static inline void socket_init_http(struct socket *sk)
{
    sk->http_length = 0;
    sk->http_parse_state = 0;
    sk->http_flags = 0;
    sk->http_ack = 0;
    sk->http_frags = 0;
}

static inline void socket_init_http_server(struct socket *sk)
{
    sk->http_length = 0;
    sk->http_parse_state = 0;
    sk->http_flags = 0;
    sk->http_ack = 0;
    sk->snd_max = sk->snd_nxt + (uint32_t)g_config.payload_size;
}

#else
#define socket_init_http(sk) do{}while(0)
#define socket_init_http_server(sk) do{}while(0)
#endif

static inline void socket_server_open(__rte_unused struct socket_table *st, struct socket *sk, struct tcphdr *th)
{
    if (sk->state != SK_SYN_RECEIVED) {
        sk->state = SK_SYN_RECEIVED;
        net_stats_socket_open();
    }

    socket_node_del(&sk->node);
#ifdef DPERF_DEBUG
    sk->log = 0;
#endif
    sk->retrans = 0;
    sk->keepalive_request_num = 0;
    sk->snd_nxt++;
    sk->snd_una = sk->snd_nxt;
    sk->snd_window = 1;
    sk->rcv_nxt = ntohl(th->th_seq) + 1;
}

static inline struct socket *socket_client_open(struct socket_table *st, uint64_t now_tsc)
{
    struct socket *sk = NULL;

    sk = socket_table_get_socket(st);
    if (sk->state == SK_CLOSED) {
#ifdef DPERF_DEBUG
        sk->log = 0;
#endif
        sk->timer_tsc = now_tsc;
        sk->retrans = 0;
        sk->keepalive_request_num = 0;
        sk->keepalive = g_config.keepalive;
        sk->snd_nxt++;
        sk->snd_una = sk->snd_nxt;
        sk->rcv_nxt = 0;
        sk->state = SK_SYN_SENT;
        net_stats_socket_open();
        socket_init_http(sk);
        return sk;
    } else {
        return NULL;
    }
}

void socket_log(struct socket *sk, const char *tag);
void socket_print(struct socket *sk, const char *tag);
int socket_table_init(struct work_space *ws);
#ifdef DPERF_DEBUG
#define SOCKET_LOG(sk, tag) socket_log(sk, tag)
#else
#define SOCKET_LOG(sk, tag)
#endif

#ifdef DPERF_DEBUG
#define SOCKET_LOG_ENABLE(sk)  do {(sk)->log = 1;} while (0)
#else
#define SOCKET_LOG_ENABLE(sk)
#endif

#endif

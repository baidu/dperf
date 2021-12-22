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

#include "socket.h"
#include "work_space.h"
#include "config.h"
#include <stdlib.h>
#include <netinet/ip.h>
#include "tcp.h"
#include "udp.h"
#include "net_stats.h"
#include <rte_malloc.h>

const char* g_sk_states[] = {
    "SK_CLOSED",
    "SK_LISTEN",
    "SK_SYN_SENT",
    "SK_SYN_RECEIVED",
    "SK_ESTABLISHED",
    "SK_CLOSE_WAIT",
    "SK_FIN_WAIT_1",
    "SK_CLOSING",
    "SK_LAST_ACK",
    "SK_FIN_WAIT_2",
    "SK_TIME_WAIT"
};

void socket_log(struct socket *sk, const char *tag)
{
    uint8_t fin = 0;
    uint8_t syn = 0;
    uint8_t ack = 0;
    uint8_t push = 0;
    FILE *log = g_work_space->log;

    fin = ((sk->flags & TH_FIN) != 0);
    syn = ((sk->flags & TH_SYN) != 0);
    ack = ((sk->flags & TH_ACK) != 0);
    push = ((sk->flags & TH_PUSH) != 0);

    fprintf(log, "sec %lu ticks %lu %s sk: "
            IPV4_FMT ":%u ->" IPV4_FMT
            ":%u syn %d fin %d push %d ack %d rcv_nxt %d snd_nxt %u snd_una %u %s\n",
            g_current_seconds, g_current_ticks,
            tag,
            IPV4_STR(sk->laddr), ntohs(sk->lport),
            IPV4_STR(sk->faddr), ntohs(sk->fport),
            syn, fin, push, ack,
            sk->rcv_nxt, sk->snd_nxt, sk->snd_una, g_sk_states[sk->state]);
}

#define L4_DATA_LEN(mcache) ((mcache)->data.l4_len + (mcache)->data.data_len)

static inline uint16_t mbuf_cal_pseudo_csum_ipv6(uint8_t proto, struct in6_addr *saddr, struct in6_addr *daddr,
    uint16_t len)
{
    struct {
        struct in6_addr saddr;
        struct in6_addr daddr;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } hdr;

    hdr.saddr = *saddr;
    hdr.daddr = *daddr;
    hdr.zero = 0;
    hdr.proto = proto;
    hdr.len = htons(len);

    return rte_raw_cksum((void *)&hdr, sizeof(hdr));
}

static inline void socket_init_pseudo_csum_ipv6(struct socket *sk, struct work_space *ws, uint8_t proto,
    struct in6_addr *laddr, struct in6_addr *faddr)
{
    if (proto == IPPROTO_TCP) {
        sk->csum_tcp = mbuf_cal_pseudo_csum_ipv6(proto, laddr, faddr, L4_DATA_LEN(&ws->tcp));
        sk->csum_tcp_opt = mbuf_cal_pseudo_csum_ipv6(proto, laddr, faddr, L4_DATA_LEN(&ws->tcp_opt));
        sk->csum_tcp_data = mbuf_cal_pseudo_csum_ipv6(proto, laddr, faddr, L4_DATA_LEN(&ws->tcp_data));
    } else {
        sk->csum_udp = mbuf_cal_pseudo_csum_ipv6(proto, laddr, faddr, L4_DATA_LEN(&ws->udp));
    }
}

static inline void socket_init_pseudo_csum(struct socket *sk, struct work_space *ws, uint8_t proto,
    uint32_t lip, uint32_t fip)
{
    if (proto == IPPROTO_TCP) {
        sk->csum_tcp = mbuf_cal_pseudo_csum(proto, lip, fip, L4_DATA_LEN(&ws->tcp));
        sk->csum_tcp_opt = mbuf_cal_pseudo_csum(proto, lip, fip, L4_DATA_LEN(&ws->tcp_opt));
        sk->csum_tcp_data = mbuf_cal_pseudo_csum(proto, lip, fip, L4_DATA_LEN(&ws->tcp_data));
    } else {
        sk->csum_udp = mbuf_cal_pseudo_csum(proto, lip, fip, L4_DATA_LEN(&ws->udp));
    }
}

static void socket_init(struct work_space *ws, struct socket *sk, uint32_t client_ip, uint16_t client_port,
     uint32_t server_ip, uint16_t server_port)
{
    uint8_t proto = ws->cfg->protocol;
    ipaddr_t caddr;
    ipaddr_t saddr;
    uint32_t seed = 0;

    sk->state = 0; /* TCP_CLOSED; */
    sk->keepalive = ws->cfg->keepalive;

    if (ws->server) {
        sk->laddr = server_ip;
        sk->faddr = client_ip;
        sk->lport = server_port;
        sk->fport = client_port;
    } else {
        sk->faddr = server_ip;
        sk->laddr = client_ip;
        sk->fport = server_port;
        sk->lport = client_port;
    }

    seed = (uint32_t)rte_rdtsc();
    sk->snd_nxt = rand_r(&seed);
    if (ws->ipv6) {
        ipaddr_join(&ws->port->client_ip_range.start, client_ip, &caddr);
        ipaddr_join(&ws->port->server_ip_range.start, server_ip, &saddr);
        socket_init_pseudo_csum_ipv6(sk, ws, proto, &caddr.in6, &saddr.in6);
    } else {
        socket_init_pseudo_csum(sk, ws, proto, client_ip, server_ip);
    }

    socket_node_init(&sk->node);
}

static struct socket_port_table *socket_port_table_new(struct work_space *ws, struct socket_table *st,
    uint32_t client_ip)
{
    int port = 0;
    uint16_t client_port = 0;
    uint16_t server_port = 0;
    struct socket *sk = NULL;
    struct socket_port_table *table = NULL;
    struct socket_pool *sp = &st->socket_pool;

    table = (struct socket_port_table *)(&(sp->base[sp->next]));
    /* skip port 0 */
    for (port = 1; port < NETWORK_PORT_NUM; port++) {
        for (server_port = st->port_min; server_port <= st->port_max; server_port++) {
            client_port = port;
            sk = socket_port_table_get(st, table, client_port, server_port);
            socket_init(ws, sk, client_ip, htons(client_port), st->server_ip, htons(server_port));
            sp->next++;
        }
    }

    return table;
}

static void socket_port_table_init_ip_range(struct work_space *ws, struct socket_table *st,
    struct ip_range *client_ip_range)
{
    int i = 0;
    uint32_t client_ip_low = 0;
    uint32_t client_ip = 0;
    struct socket_port_table *pt = NULL;

    for (i = 0; i < client_ip_range->num; i++) {
        client_ip = ip_range_get(client_ip_range, i);
        pt = socket_port_table_new(ws, st, client_ip);
        client_ip_low = ntohl(client_ip) & 0xffff;
        st->ht[client_ip_low] = pt;
    }
}

static void socket_port_table_init_ip_group(struct work_space *ws, struct socket_table *st,
    struct ip_group *ip_group)
{
    struct ip_range *ip_range = NULL;

    for_each_ip_range(ip_group, ip_range) {
        socket_port_table_init_ip_range(ws, st, ip_range);
    }
}

void socket_table_init(struct work_space *ws)
{
    struct config *cfg = ws->cfg;
    struct netif_port *port = ws->port;
    struct socket_table *st = &ws->socket_table;

    st->server_ip = ip_range_get(&port->server_ip_range, ws->queue_id);
    st->port_min = cfg->listen;
    st->port_max = cfg->listen + cfg->listen_num - 1;
    st->port_num = cfg->listen_num;

    if (cfg->server) {
        socket_port_table_init_ip_group(ws, st, &cfg->client_ip_group);
    } else {
        socket_port_table_init_ip_range(ws, st, &(port->client_ip_range));
    }

    st->socket_pool.next = 0;
}

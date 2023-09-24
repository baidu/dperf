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

#include "socket.h"
#include "work_space.h"
#include "config.h"
#include <stdlib.h>
#include <netinet/ip.h>
#include "tcp.h"
#include "udp.h"
#include "net_stats.h"
#include "csum.h"
#include "rss.h"
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
            ":%u syn %d fin %d push %d ack %d rcv_nxt %u snd_nxt %u snd_una %u %s\n",
            g_current_seconds, g_current_ticks,
            tag,
            IPV4_STR(sk->laddr), ntohs(sk->lport),
            IPV4_STR(sk->faddr), ntohs(sk->fport),
            syn, fin, push, ack,
            sk->rcv_nxt, sk->snd_nxt, sk->snd_una, g_sk_states[sk->state]);
}

void socket_print(struct socket *sk, const char *tag)
{
    FILE *log = g_work_space->log;

    g_work_space->log = stdout;
    socket_log(sk, tag);
    g_work_space->log = log;
}

static void socket_init(struct work_space *ws, struct socket *sk, uint32_t client_ip, uint16_t client_port,
     uint32_t server_ip, uint16_t server_port)
{
    uint32_t seed = 0;
    struct config *cfg = NULL;

    cfg = ws->cfg;
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

    csum_init_socket(ws, sk);
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
    for (port = st->client_port_min; port <= st->client_port_max; port++) {
        for (server_port = st->server_port_min; server_port <= st->server_port_max; server_port++) {
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

static int socket_table_init_client_rss_l3(struct work_space *ws)
{
    int ret = -1;
    unsigned int i = 0;
    unsigned int j = 0;
    unsigned int step = 0;
    struct socket *sk = NULL;
    struct socket_table *st = NULL;
    struct socket_pool *sp = NULL;

    st = &ws->socket_table;
    sp = &st->socket_pool;
    step = st->client_port_num * st->server_port_num;

    while (i < sp->num) {
        sk = &(sp->base[i]);
        if (!rss_check_socket(ws, sk)) {
            for (j = 0; j < step; j++) {
                sk = &(sp->base[i + j]);
                sk->laddr = 0;
                sk->faddr = 0;
            }
        } else {
            ret = 0;
        }
        i += step;
    }

    return ret;
}

static int socket_table_init_client_rss_l3l4(struct work_space *ws)
{
    unsigned int i = 0;
    struct socket *sk = NULL;
    struct socket_table *st = NULL;
    struct socket_pool *sp = NULL;

    st = &ws->socket_table;
    sp = &st->socket_pool;
    for (i = 0; i < sp->num; i++) {
        sk = &(sp->base[i]);
        if (!rss_check_socket(ws, sk)) {
            sk->laddr = 0;
            sk->faddr = 0;
        }
    }

    return 0;
}

static int socket_table_init_client_rss(struct work_space *ws)
{
    if (ws->cfg->rss == RSS_L3) {
        return socket_table_init_client_rss_l3(ws);
    } else if (ws->cfg->rss == RSS_L3L4) {
        return socket_table_init_client_rss_l3l4(ws);
    }  else if (ws->cfg->rss == RSS_AUTO) {
        return 0;
    } else {
        return -1;
    }
}

int socket_table_init(struct work_space *ws)
{
    struct config *cfg = ws->cfg;
    struct netif_port *port = ws->port;
    struct socket_table *st = &ws->socket_table;

    st->server_ip = ip_range_get(&port->server_ip_range, ws->queue_id);
    st->server_port_min = cfg->listen;
    st->server_port_max = cfg->listen + cfg->listen_num - 1;
    st->server_port_num = cfg->listen_num;

    st->client_port_min = cfg->lport_min;
    st->client_port_max = cfg->lport_max;
    st->client_port_num = cfg->lport_max - cfg->lport_min + 1;

    if (cfg->client_hop) {
        st->client_hop = 1;
    }

    if (cfg->server) {
        socket_port_table_init_ip_group(ws, st, &cfg->client_ip_group);
    } else {
        socket_port_table_init_ip_range(ws, st, &(port->client_ip_range));
        if ((cfg->rss != RSS_NONE) && (socket_table_init_client_rss(ws) < 0)) {
            printf("Error: worker %d has no client address, please increase the number of client address\n", ws->id);
            return -1;
        }
    }

    st->socket_pool.next = 0;
    return 0;
}

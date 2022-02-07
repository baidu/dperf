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

#ifndef __WORK_SPACE_H
#define __WORK_SPACE_H

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include "config.h"
#include "cpuload.h"
#include "mbuf.h"
#include "mbuf_cache.h"
#include "net_stats.h"
#include "tcp.h"
#include "tick.h"
#include "socket.h"
#include "csum.h"

struct socket_table;

extern __thread struct work_space *g_work_space;
#define g_current_ticks (g_work_space->time.tick.count)
#define g_current_seconds (g_work_space->time.second.count)

#define work_space_ticks(ws) (ws->time.tick.count)
#define work_space_seconds(ws) (ws->time.second.count)

struct tx_queue {
    uint16_t head;
    uint16_t tail;
    uint16_t tx_burst;
    struct rte_mbuf *tx[TX_QUEUE_SIZE];
};

/* launch every tick */
struct client_launch {
    uint64_t cc;
    uint64_t launch_next;
    uint64_t launch_interval;
    uint64_t launch_interval_default;
    uint32_t launch_num;
};

struct work_space {
    union {
        /* read only */
        struct {
            const uint8_t id;
            const uint8_t ipv6:1;
            const uint8_t server:1;
            const uint8_t port_id;
            const uint8_t queue_id;
        };
        /* write once */
        struct {
            uint8_t w_id;
            uint8_t w_ipv6:1;
            uint8_t w_server:1;
            uint8_t w_port_id;
            uint8_t w_queue_id;
        };
    };

    uint16_t ip_id;
    bool exit;
    bool stop;
    uint32_t vni:24;
    uint32_t vxlan:8;
    uint32_t vtep_ip; /* each queue has a vtep ip */
    struct tick_time time;
    struct cpuload load;
    struct client_launch client_launch;

    struct mbuf_cache tcp_opt;
    struct mbuf_cache tcp_data;
    union {
        struct mbuf_cache udp;
        struct mbuf_cache tcp;
    };

    FILE *log;
    struct config *cfg;
    struct netif_port *port;
    void (*run_loop)(struct work_space *ws);

    struct tx_queue tx_queue;
    struct rte_mbuf *mbuf_rx[NB_RXD];
    struct socket_table socket_table;
};

static inline bool work_space_in_duration(struct work_space *ws)
{
    if ((ws->time.second.count < (uint64_t)(g_config.duration)) && (ws->stop == false)) {
        return true;
    } else {
        return false;
    }
}

static inline void work_space_tx_flush(struct work_space *ws)
{
    int i = 0;
    int n = 0;
    int num = 0;
    struct tx_queue *queue = &ws->tx_queue;
    struct rte_mbuf **tx = NULL;

    if (queue->head == queue->tail) {
        return;
    }

    for (i = 0; i < 8; i++) {
        num = queue->tail - queue->head;
        if (num > queue->tx_burst) {
            num = queue->tx_burst;
        }

        tx = &queue->tx[queue->head];
        n = rte_eth_tx_burst(g_work_space->port_id, g_work_space->queue_id, tx, num);
        queue->head += n;
        if (queue->head == queue->tail) {
            queue->head = 0;
            queue->tail = 0;
            return;
        } else if (queue->tail < TX_QUEUE_SIZE) {
            return;
        }
    }

    num = queue->tail - queue->head;
    net_stats_tx_drop(num);
    for (i = queue->head; i < queue->tail; i++) {
        rte_pktmbuf_free(queue->tx[i]);
    }
    queue->head = 0;
    queue->tail = 0;
}

static inline void work_space_tx_send(struct work_space *ws, struct rte_mbuf *mbuf)
{
    struct tx_queue *queue = &ws->tx_queue;

    net_stats_tx(mbuf);
    queue->tx[queue->tail] = mbuf;
    queue->tail++;
    if (((queue->tail - queue->head) >= queue->tx_burst) || (queue->tail == TX_QUEUE_SIZE)) {
        work_space_tx_flush(ws);
    }
}

static inline void work_space_tx_send_tcp(struct work_space *ws, struct rte_mbuf *mbuf)
{
    uint64_t ol_flags = PKT_TX_TCP_CKSUM;

    if (ws->vxlan) {
        ol_flags = PKT_TX_UDP_CKSUM;
    }

    csum_offload_ip_tcpudp(mbuf, ol_flags);
    work_space_tx_send(ws, mbuf);
}

static inline void work_space_tx_send_udp(struct work_space *ws, struct rte_mbuf *mbuf)
{
    csum_offload_ip_tcpudp(mbuf, PKT_TX_UDP_CKSUM);
    work_space_tx_send(ws, mbuf);
}

static inline uint64_t work_space_client_launch_num(struct work_space *ws)
{
    struct tick_time *tt = &ws->time;
    uint64_t launch_num = ws->client_launch.launch_num;
    uint64_t num = launch_num;
    uint64_t gap = 0;
    uint64_t cc = ws->client_launch.cc;

    if (ws->client_launch.launch_next <= tt->tsc) {
        ws->client_launch.launch_next += ws->client_launch.launch_interval;
        if (cc > 0) {
            if (g_net_stats.socket_current < cc) {
                gap = cc - g_net_stats.socket_current;
                if (gap < launch_num) {
                    num = gap;
                }
            } else {
                num = 0;
            }
        }

        return num;
    } else {
        return 0;
    }
}

void work_space_stop_all(void);
void work_space_exit_all(void);
struct work_space *work_space_new(struct config *cfg, int id);
void work_space_close(struct work_space *ws);
bool work_space_ip_exist(const struct work_space *ws, uint32_t ip);
bool work_space_ip6_exist(const struct work_space *ws, const ipaddr_t *addr);
void work_space_update_gw(struct work_space *ws, struct eth_addr *ea);
struct rte_mbuf *work_space_alloc_mbuf(struct work_space *ws);
void work_space_set_launch_interval(uint64_t launch_interval);

#endif

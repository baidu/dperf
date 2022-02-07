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

#include "work_space.h"
#include "mbuf.h"
#include "socket.h"
#include "socket_timer.h"
#include "net_stats.h"
#include "client.h"
#include "server.h"
#include "udp.h"

#include <rte_cycles.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <pthread.h>

__thread struct work_space *g_work_space;
static struct work_space *g_work_space_all[THREAD_NUM_MAX];
static rte_atomic32_t g_wait_count;

static void work_space_wait_all(struct work_space *ws)
{
    int num = ws->cfg->cpu_num;
    int val = 0;

    rte_atomic32_inc(&g_wait_count);
    while (1) {
        val = rte_atomic32_read(&g_wait_count);
        if ((val > 0) && ((val % num) == 0)) {
            break;
        }
    }
}

static void work_space_get_port(struct work_space *ws)
{
    struct config *cfg = ws->cfg;
    struct netif_port *port = NULL;
    struct vxlan *vxlan = NULL;
    int queue_id = 0;

    port = config_port_get(cfg, ws->id, &queue_id);
    ws->w_queue_id = queue_id;
    ws->port = port;
    ws->w_port_id = port->id;

    if (cfg->vxlan) {
        ws->vxlan = cfg->vxlan;
        vxlan = port->vxlan;
        ws->vtep_ip = ip_range_get(&vxlan->vtep_local, ws->queue_id);
        ws->vni = VXLAN_HTON(vxlan->vni);
    }
}

static int work_space_open_log(struct work_space *ws)
{
    FILE *fp = NULL;
#ifdef DPERF_DEBUG
    char name[64];

    if (ws->cfg->server) {
        sprintf(name, "%s/dperf-server-%d.log", LOG_DIR, ws->id);
    } else {
        sprintf(name, "%s/dperf-client-%d.log", LOG_DIR, ws->id);
    }

    fp = fopen(name, "a");
    if (fp == NULL) {
        printf("open file %s error\n", name);
        return -1;
    }
#endif
    ws->log = fp;

    return 0;
}

static void work_space_close_log(struct work_space *ws)
{
    if (ws && ws->log) {
        fclose(ws->log);
        ws->log = NULL;
    }
}

static void work_space_init_time(struct work_space *ws)
{
    uint32_t cpu_num = 0;
    uint64_t us = 0;

    cpu_num = ws->cfg->cpu_num;
    us = (1000ul * 1000ul * 1000ul) / (TICKS_PER_SEC * cpu_num);

    work_space_wait_all(ws);
    usleep(ws->id * us);

    socket_timer_init();
    tick_time_init(&ws->time);
}

static struct work_space *work_space_alloc(struct config *cfg, int id)
{
    size_t size = 0;
    uint32_t socket_num = 0;
    struct work_space *ws = NULL;

    socket_num = config_get_total_socket_num(cfg, id);
    size = sizeof(struct work_space) + socket_num * sizeof(struct socket);

    ws = (struct work_space *)rte_calloc("work_space", 1, size, CACHE_ALIGN_SIZE);
    if (ws != NULL) {
        printf("socket allocation succeeded, size %0.2fGB num %u\n", size * 1.0 / (1024 * 1024 * 1024), socket_num);
        ws->socket_table.socket_pool.num = socket_num;
    } else {
        printf("socket allocation failed, size %0.2fGB num %u\n", size * 1.0 / (1024 * 1024 * 1024), socket_num);
    }

    return ws;
}

struct work_space *work_space_new(struct config *cfg, int id)
{
    struct work_space *ws = NULL;

    ws = work_space_alloc(cfg, id);
    if (ws == NULL) {
        return NULL;
    }

    g_work_space = ws;
    g_work_space_all[id] = ws;
    ws->w_server = cfg->server;
    ws->w_id = id;
    ws->w_ipv6 = cfg->af == AF_INET6;
    ws->cfg = cfg;
    ws->tx_queue.tx_burst = cfg->tx_burst;
    work_space_get_port(ws);

    tcp_init(ws);
    udp_init(ws);
    if (work_space_open_log(ws) < 0) {
        goto err;
    }

    work_space_init_time(ws);
    cpuload_init(&ws->load);
    socket_table_init(ws);
    net_stats_init(ws);

    if (cfg->server) {
        server_init(ws);
    } else {
        client_init(ws);
    }

    work_space_wait_all(ws);
    return ws;

err:
    work_space_close(ws);
    return NULL;
}

void work_space_close(struct work_space *ws)
{
    work_space_close_log(ws);
}

bool work_space_ip_exist(const struct work_space *ws, uint32_t ip)
{
    struct netif_port *port = NULL;
    struct vxlan *vxlan = NULL;

    port = ws->port;

    if ((!port->ipv6) && (port->local_ip.ip == ip)) {
        return true;
    }

    if (ws->vxlan) {
        vxlan = port->vxlan;
        return ip_range_exist(&vxlan->vtep_local, ip);
    }

    if (!ws->ipv6) {
        return ip_range_exist(port->local_ip_range, ip);
    }

    return false;
}

bool work_space_ip6_exist(const struct work_space *ws, const ipaddr_t *addr)
{
    struct netif_port *port = NULL;
    struct ip_range *ip_range = NULL;
    ipaddr_t *laddr = NULL;

    port = ws->port;
    ip_range = port->local_ip_range;
    laddr = &port->local_ip;

    if (ws->port->ipv6) {
        if (ipaddr_eq(laddr, addr)) {
            return true;
        }
    }

    if (ws->ipv6) {
        return ip_range_exist_ipv6(ip_range, addr);
    }

    return false;
}

void work_space_update_gw(struct work_space *ws, struct eth_addr *ea)
{
    int i = 0;
    struct netif_port *port = NULL;

    port = ws->port;
    eth_addr_copy(&port->gateway_mac, ea);
    for (i = 0; i < THREAD_NUM_MAX; i++) {
        ws = g_work_space_all[i];
        if ((ws == NULL) || (ws->port != port)) {
            continue;
        }

        mbuf_cache_set_dmac(&ws->tcp, ea);
        mbuf_cache_set_dmac(&ws->tcp_opt, ea);
        mbuf_cache_set_dmac(&ws->tcp_data, ea);
        mbuf_cache_set_dmac(&ws->udp, ea);
    }

    printf("Get gateway's MAC address successfully\n");
}

struct rte_mbuf *work_space_alloc_mbuf(struct work_space *ws)
{
    struct rte_mempool *p = NULL;

    p = port_get_mbuf_pool(ws->port, ws->queue_id);
    if (p) {
        return rte_pktmbuf_alloc(p);
    }

    return NULL;
}

void work_space_stop_all(void)
{
    int i = 0;
    struct work_space *ws = NULL;

    for (i = 0; i < THREAD_NUM_MAX; i++) {
        ws = g_work_space_all[i];
        if (ws != NULL) {
            ws->client_launch.launch_num = 0;
            ws->stop = true;
        }
    }
}

void work_space_exit_all(void)
{
    int i = 0;
    struct work_space *ws = NULL;

    for (i = 0; i < THREAD_NUM_MAX; i++) {
        ws = g_work_space_all[i];
        if (ws != NULL) {
            ws->exit = true;
        }
    }
}

void work_space_set_launch_interval(uint64_t launch_interval)
{
    int i = 0;
    struct work_space *ws = NULL;
    uint64_t interval = 0;

    for (i = 0; i < THREAD_NUM_MAX; i++) {
        ws = g_work_space_all[i];
        if (ws == NULL) {
            continue;
        }

        interval = launch_interval;
        if (interval >= g_tsc_per_second) {
            interval = g_tsc_per_second;
        }

        if (interval <= ws->client_launch.launch_interval_default) {
            interval = ws->client_launch.launch_interval_default;
        }

        ws->client_launch.launch_interval = interval;
    }
}

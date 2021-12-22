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

#ifndef __CONFIG_H
#define __CONFIG_H

#include "ip_range.h"
#include "port.h"
#include "eth.h"

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define RTE_ARG_LEN         64
#define CACHE_ALIGN_SIZE    64
#define MSS_MAX             1460
#define NETWORK_PORT_NUM    65536
#define PAYLOAD_SIZE_MAX    1400
#define DEFAULT_CPS         1000
#define DEFAULT_INTERVAL    1       /* 1s */
#define DEFAULT_DURATION    60
#define DEFAULT_TTL         64
#define ND_TTL              255
#define DEFAULT_LAUNCH      4
#define DELAY_SEC           4
#define SLOW_START_SEC      10
#define LOG_DIR             "/var/log/dperf"

struct config {
    bool server;
    bool keepalive;
    bool ipv6;
    bool daemon;
    bool synflood;
    uint8_t tx_burst;
    uint8_t protocol;   /* TCP/UDP */
    int af;
    int keepalive_request_interval;
    int keepalive_request_num;

    int payload_size;
    int mss;

    uint32_t launch_num;
    int duration;
    int cps;        /* connection per seconds */
    int cc;         /* current connections */

    int cpu[THREAD_NUM_MAX];
    int cpu_num;

    /*
     * Running multiple instances on one host
     * dpdk rte_eal_init() --socket-meme
     * */
    char socket_mem[RTE_ARG_LEN];

    struct netif_port ports[NETIF_PORT_MAX];
    int port_num;

    int listen;
    int listen_num;

    struct ip_group client_ip_group;
    struct ip_group server_ip_group;
};

#define config_for_each_port(cfg, port) \
    for ((port) = &((cfg)->ports[0]); ((port) - &((cfg)->ports[0])) < (cfg)->port_num; port++)

extern struct config g_config;
int config_parse(int argc, char **argv, struct config *cfg);
uint32_t config_get_total_socket_num(struct config *cfg, int id);
struct netif_port *config_port_get(struct config *cfg, int thread_id, int *p_queue_id);

#endif


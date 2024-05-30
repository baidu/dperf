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

#ifndef __CONFIG_H
#define __CONFIG_H

#include "ip_range.h"
#include "ip_list.h"
#include "port.h"
#include "eth.h"

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

#define RTE_ARG_LEN         64
#define CACHE_ALIGN_SIZE    64
#define TCP_WIN             (1460 * 40)
#define NETWORK_PORT_NUM    65536

#define PACKET_SIZE_MAX     1514
#define DEFAULT_CPS         1000
#define DEFAULT_INTERVAL    1       /* 1s */
#define DEFAULT_DURATION    60
#define DEFAULT_TTL         64
#define ND_TTL              255
#define DEFAULT_LAUNCH      4
#define DELAY_SEC           4
#define NEIGH_SEC           60
#define WAIT_DEFAULT        3
#define SLOW_START_DEFAULT  30
#define SLOW_START_MIN      10
#define SLOW_START_MAX      600
#define KEEPALIVE_REQ_NUM   32767  /* 15 bits */

#define JUMBO_FRAME_SIZE(mtu)   ((mtu) + 14 + ETHER_CRC_LEN)
#define JUMBO_PKT_SIZE(mtu)     ((mtu) + 14)
#define JUMBO_MTU_MIN       9000
#define JUMBO_MTU_MAX       9710
#define JUMBO_MTU_DEFAULT   JUMBO_MTU_MAX
#define JUMBO_MBUF_SIZE     (1024 * 11)
#define MBUF_DATA_SIZE      (1024 * 10)

#define MSS_IPV4            (PACKET_SIZE_MAX - 14 - 20 - 20)
#define MSS_IPV6            (PACKET_SIZE_MAX - 14 - 40 - 20)
#define MSS_JUMBO_IPV4(mtu) ((mtu) - 20 - 20)
#define MSS_JUMBO_IPV6(mtu) ((mtu) - 40 - 20)

#define DEFAULT_WSCALE      13

#define LOG_DIR             "/var/log/dperf"

#define HTTP_HOST_MAX       128
#define HTTP_PATH_MAX       256
#define PAYLOAD_SIZE_MAX    (1L * 1024 * 1024 * 1024)
#define SEND_WINDOW_MAX     16
#define SEND_WINDOW_MIN     2
#define SEND_WINDOW_DEFAULT 4

#define HTTP_HOST_DEFAULT   "dperf"
#define HTTP_PATH_DEFAULT   "/"

#define HTTP_METH_GET       0
#define HTTP_METH_POST      1

#define TCP_ACK_DELAY_MAX   1024

#define RSS_NONE            0
#define RSS_L3              1
#define RSS_L3L4            2
#define RSS_AUTO            3

#define KNI_NAMESIZE        10

#define VLAN_ID_MIN         1
#define VLAN_ID_MAX         4094

#define PIPELINE_MIN        0
#define PIPELINE_MAX        100
#define PIPELINE_DEFAULT    0

#define LOG_LEVEL_DEFAULT   4
#define LOG_LEVEL_ERR       4
#define LOG_LEVEL_WARN      5
#define LOG_LEVEL_INFO      7
#define LOG_LEVEL_DEBUG     8

/* retransmit timeout */
#define RTO_DEFAULT         2
#define RTO_MIN             2
#define RTO_MAX             300

struct config {
    bool server;
    bool keepalive;
    bool ipv6;
    bool vxlan;
    bool kni;
    bool daemon;
    bool flood;
    bool jumbo;
    bool payload_random;
    bool client_hop;
    bool simd512;
    bool fast_close;
    bool clear_screen;
    bool disable_ack;
    uint8_t log_level;
    uint8_t rss;
    bool mq_rx_rss;
    uint8_t rss_auto;
    bool quiet;
    bool tcp_rst;
    bool http;
    bool stats_http;    /* payload size >= HTTP_DATA_MIN_SIZE */
    uint8_t http_method;
    uint8_t tos;
    uint8_t pipeline;
    uint8_t tx_burst;
    uint8_t send_window;/* packets */
    uint8_t protocol;   /* TCP/UDP */
    uint16_t vlan_id;
    uint16_t jumbo_mtu;

    int ticks_per_sec;

    int lport_min;
    int lport_max;

    char kni_ifname[KNI_NAMESIZE];
    int af;

    uint32_t retransmit_timeout_sec;
    /* tsc */
    uint64_t retransmit_timeout;
    uint64_t keepalive_request_interval_us;
    /* tsc */
    uint64_t keepalive_request_interval;
    int keepalive_request_num;

    char http_host[HTTP_HOST_MAX];
    char http_path[HTTP_PATH_MAX];

    int payload_size;
    int packet_size;
    int mss;

    int wait;
    int slow_start;
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

    struct vxlan vxlans[NETIF_PORT_MAX];
    int vxlan_num;

    int listen;
    int listen_num;

    struct ip_group client_ip_group;
    struct ip_group server_ip_group;
    struct ip_list  dip_list;
};

#define config_for_each_port(cfg, port) \
    for ((port) = &((cfg)->ports[0]); ((port) - &((cfg)->ports[0])) < (cfg)->port_num; port++)

extern struct config g_config;
int config_parse(int argc, char **argv, struct config *cfg);
uint32_t config_get_total_socket_num(struct config *cfg, int id);
struct netif_port *config_port_get(struct config *cfg, int thread_id, int *p_queue_id);
void config_set_tsc(struct config *cfg, uint64_t hz);
void config_set_payload(struct config *cfg, char *data, int len, int new_line);

#endif


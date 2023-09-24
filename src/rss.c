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

#include "rss.h"
#include <stdbool.h>
#include <rte_ethdev.h>
#include <rte_thash.h>
#include "config.h"
#include "work_space.h"
#include "socket.h"
#include "ip.h"

#define RSS_HASH_KEY_LENGTH 40
static uint8_t rss_hash_key_symmetric_be[RSS_HASH_KEY_LENGTH];
static uint8_t rss_hash_key_symmetric[RSS_HASH_KEY_LENGTH] = {
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
};

static uint64_t rss_get_rss_hf(struct rte_eth_dev_info *dev_info, uint8_t rss)
{
    uint64_t offloads = 0;
    uint64_t ipv4_flags = 0;
    uint64_t ipv6_flags = 0;

    offloads = dev_info->flow_type_rss_offloads;
    if (rss == RSS_L3) {
        ipv4_flags = RTE_ETH_RSS_IPV4 | RTE_ETH_RSS_FRAG_IPV4;
        ipv6_flags = RTE_ETH_RSS_IPV6 | RTE_ETH_RSS_FRAG_IPV6;
    } else if (rss == RSS_L3L4) {
        ipv4_flags = RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV4_TCP;
        ipv6_flags = RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_NONFRAG_IPV6_TCP;
    }

    if (g_config.ipv6) {
        if ((offloads & ipv6_flags) == 0) {
            return 0;
        }
    } else {
        if ((offloads & ipv4_flags) == 0) {
            return 0;
        }
    }

    return (offloads & (ipv4_flags | ipv6_flags));
}

int rss_config_port(struct rte_eth_conf *conf, struct rte_eth_dev_info *dev_info)
{
    uint64_t rss_hf = 0;
    struct rte_eth_rss_conf *rss_conf = NULL;

    /* no need to configure hardware */
    if (g_config.flood) {
        return 0;
    }

    rss_conf = &conf->rx_adv_conf.rss_conf;
    if (g_config.rss == RSS_AUTO) {
        if (g_config.mq_rx_rss) {
            conf->rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
            rss_conf->rss_hf = rss_get_rss_hf(dev_info, g_config.rss_auto);
        }
        return 0;
    }

    rss_hf = rss_get_rss_hf(dev_info, g_config.rss);
    if (rss_hf == 0) {
        return -1;
    }

    conf->rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
    rss_conf->rss_key = rss_hash_key_symmetric;
    rss_conf->rss_key_len = RSS_HASH_KEY_LENGTH,
    rss_conf->rss_hf = rss_hf;

    return 0;
}

static uint32_t rss_hash_data(uint32_t *data, int len)
{
    int i = 0;

    for (i = 0; i < len; i++) {
        data[i] = rte_be_to_cpu_32(data[i]);
    }

    return rte_softrss_be(data, len, rss_hash_key_symmetric_be);
}

static uint32_t rss_hash_socket_ipv4(struct work_space *ws, struct socket *sk)
{
    int len = 2;
    struct rte_ipv4_tuple tuple;

    tuple.src_addr = sk->faddr;
    tuple.dst_addr = sk->laddr;
    if (ws->cfg->rss == RSS_L3L4) {
        tuple.dport = sk->lport;
        tuple.sport = sk->fport;
        len++;
    }

    return rss_hash_data((uint32_t *)&tuple, len);
}

static uint32_t rss_hash_socket_ipv6(struct work_space *ws, struct socket *sk)
{
    int len = 8;
    ipaddr_t laddr;
    ipaddr_t faddr;
    struct rte_ipv6_tuple tuple;
    struct netif_port *port = NULL;

    port = ws->port;
    if (ws->cfg->server) {
        ipaddr_join(&port->client_ip_range.start, sk->faddr, &faddr);
        ipaddr_join(&port->server_ip_range.start, sk->laddr, &laddr);
    } else {
        ipaddr_join(&port->client_ip_range.start, sk->laddr, &laddr);
        ipaddr_join(&port->server_ip_range.start, sk->faddr, &faddr);
    }
    memcpy(tuple.src_addr, &laddr, 16);
    memcpy(tuple.dst_addr, &faddr, 16);

    if (ws->cfg->rss == RSS_L3L4) {
        tuple.dport = sk->fport;
        tuple.sport = sk->lport;
        len++;
    }

    return rss_hash_data((uint32_t *)&tuple, len);
}

bool rss_check_socket(struct work_space *ws, struct socket *sk)
{
    uint32_t hash = 0;

    if (ws->ipv6) {
        hash = rss_hash_socket_ipv6(ws, sk);
    } else {
        hash = rss_hash_socket_ipv4(ws, sk);
    }
    hash = hash % ws->port->queue_num;
    if (hash == ws->queue_id) {
        return true;
    }

    return false;
}

void rss_init(void)
{
    rte_convert_rss_key((const uint32_t *)rss_hash_key_symmetric,
                        (uint32_t *)rss_hash_key_symmetric_be, RSS_HASH_KEY_LENGTH);
}

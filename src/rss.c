/*
 * Copyright (c) 2022 Baidu.com, Inc. All Rights Reserved.
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

#include "rss.h"
#include <stdbool.h>
#include <rte_ethdev.h>
#include <rte_thash.h>
#include "config.h"
#include "work_space.h"
#include "socket.h"

#define RSS_HASH_KEY_LENGTH 40
static uint8_t rss_hash_key_symmetric_be[RSS_HASH_KEY_LENGTH];
static uint8_t rss_hash_key_symmetric[RSS_HASH_KEY_LENGTH] = {
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
    0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A, 0x6D, 0x5A,
};

static uint64_t rss_get_rss_hf(struct rte_eth_dev_info *dev_info)
{
    uint64_t offloads = 0;
    uint64_t ipv4_flags = 0;
    uint64_t ipv6_flags = 0;

    offloads = dev_info->flow_type_rss_offloads;
    ipv4_flags = ETH_RSS_IPV4 |ETH_RSS_FRAG_IPV4;
    ipv6_flags = ETH_RSS_IPV6 | ETH_RSS_FRAG_IPV6;

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

    rss_hf = rss_get_rss_hf(dev_info);
    if (rss_hf == 0) {
        return -1;
    }

    conf->rxmode.mq_mode = ETH_MQ_RX_RSS;
    rss_conf = &conf->rx_adv_conf.rss_conf;
    rss_conf->rss_key = rss_hash_key_symmetric;
    rss_conf->rss_key_len = RSS_HASH_KEY_LENGTH,
    rss_conf->rss_hf = rss_hf;

    return 0;
}

static uint32_t rss_hash_socket_ipv4(__rte_unused struct work_space *ws, struct socket *sk)
{
    uint32_t input[2];
    uint32_t hash = 0;

    input[0] = rte_be_to_cpu_32(sk->laddr);
    input[1] = rte_be_to_cpu_32(sk->faddr);
    hash = rte_softrss_be(input, 2, rss_hash_key_symmetric_be);
    return hash;
}

static uint32_t rss_hash_socket_ipv6(struct work_space *ws, struct socket *sk)
{
    int i = 0;
    int len = 8;
    struct {
        ipaddr_t lip;
        ipaddr_t fip;
    } input;
    uint32_t *data = NULL;
    uint32_t hash = 0;

    if (ws->cfg->server) {
        input.fip = ws->port->client_ip_range.start;
        input.lip = ws->port->server_ip_range.start;
    } else {
        input.lip = ws->port->client_ip_range.start;
        input.fip = ws->port->server_ip_range.start;
    }

    input.lip.ip = sk->laddr;
    input.fip.ip = sk->faddr;

    data = (uint32_t *)&input;
    for (i = 0; i < len; i++) {
        data[i] = rte_be_to_cpu_32(data[i]);
    }

    hash = rte_softrss_be(data, len, rss_hash_key_symmetric_be);
    return hash;
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

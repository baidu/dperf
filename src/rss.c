/*
 * Copyright (c) 2021-2022 Baidu.com, Inc. All Rights Reserved.
 * Copyright (c) 2022-2024 Jianzhang Peng. All Rights Reserved.
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

static uint64_t rss_get_rss_hf(struct rte_eth_dev_info *dev_info)
{
    uint64_t offloads = 0;
    uint64_t ipv4_flags = 0;
    uint64_t ipv6_flags = 0;

    offloads = dev_info->flow_type_rss_offloads;
    ipv4_flags = RTE_ETH_RSS_NONFRAG_IPV4_UDP | RTE_ETH_RSS_NONFRAG_IPV4_TCP;
    ipv6_flags = RTE_ETH_RSS_NONFRAG_IPV6_UDP | RTE_ETH_RSS_NONFRAG_IPV6_TCP;

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

void rss_config_port(struct rte_eth_conf *conf, struct rte_eth_dev_info *dev_info, int mq_mode)
{
    struct rte_eth_rss_conf *rss_conf = NULL;

    rss_conf = &conf->rx_adv_conf.rss_conf;
    if (mq_mode) {
        conf->rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;
        rss_conf->rss_hf = rss_get_rss_hf(dev_info);
    } else {
        conf->rxmode.mq_mode = 0;
        rss_conf->rss_hf = 0;
    }
}

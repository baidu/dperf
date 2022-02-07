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

#include "icmp.h"
#include "mbuf.h"
#include "work_space.h"

/*
 * 1. reply to ping request
 * 2. drop others
 * */
void icmp_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct iphdr *iph = mbuf_ip_hdr(m);
    struct icmphdr *icmph = mbuf_icmp_hdr(m);

    net_stats_icmp_rx();
    if (icmph->type != ICMP_ECHO) {
        mbuf_free(m);
        return;
    }

    if (!work_space_ip_exist(ws, iph->daddr)) {
        mbuf_free(m);
        return;
    }

    icmph->type = ICMP_ECHOREPLY;
    icmph->checksum = icmph->checksum + htons(0x0800);

    iph_swap_addr(iph);

    iph->ttl = DEFAULT_TTL;
    /* hns3 dose not support IP csum offload */
    csum_ip_compute(m);

    eth_addr_swap(eth);
    work_space_tx_send(ws, m);
    net_stats_icmp_tx();
}

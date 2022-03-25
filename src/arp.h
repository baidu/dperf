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

#ifndef __ARP_H
#define __ARP_H

#include <rte_mbuf.h>

#include "eth.h"

struct arphdr {
    uint16_t  ar_hrd;
    uint16_t  ar_pro;
    uint8_t   ar_hln;
    uint8_t   ar_pln;

    uint16_t  ar_op;

    struct eth_addr ar_sha;
    uint32_t  ar_sip;
    struct eth_addr ar_tha;
    uint32_t  ar_tip;
} __attribute__((__packed__));

#define ARP_REQUEST    1
#define ARP_REPLY      2

struct work_space;
void arp_request_gw(struct work_space *ws);
void arp_process(struct work_space *ws, struct rte_mbuf *m);
void arp_send(struct work_space *ws, struct rte_mbuf *m);

#endif

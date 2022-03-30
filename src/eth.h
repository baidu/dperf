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

#ifndef __ETH_H
#define __ETH_H

#include <arpa/inet.h>
#include <sys/types.h>
#include <rte_ether.h>
#include "dpdk.h"

#define ETH_ADDR_LEN        6
#define ETH_ADDR_STR_LEN    17

struct eth_addr {
    uint8_t bytes[ETH_ADDR_LEN];
} __attribute__((__packed__));

struct eth_hdr {
    struct eth_addr d_addr;
    struct eth_addr s_addr;
    uint16_t type;
} __attribute__((__packed__));

static inline int eth_addr_is_zero(const struct eth_addr *ea)
{
    return ((ea)->bytes[0] == 0) && ((ea)->bytes[1] == 0) && ((ea)->bytes[2] == 0) &&
            ((ea)->bytes[3] == 0) && ((ea)->bytes[4] == 0) && ((ea)->bytes[5] == 0);
}

static inline void eth_addr_copy(struct eth_addr *dst, const struct eth_addr *src)
{
    memcpy((void*)dst, (const void*)src, sizeof(struct eth_addr));
}

static inline void eth_addr_swap(struct eth_hdr *eth)
{
    struct eth_addr addr;

    eth_addr_copy(&addr, &eth->d_addr);
    eth_addr_copy(&eth->d_addr, &eth->s_addr);
    eth_addr_copy(&eth->s_addr, &addr);
}

static inline void eth_hdr_set(struct eth_hdr *eth, uint16_t type, const struct eth_addr *d_addr,
    const struct eth_addr *s_addr)
{
    eth->type = htons(type);
    eth_addr_copy(&eth->d_addr, d_addr);
    eth_addr_copy(&eth->s_addr, s_addr);
}

void eth_addr_to_str(const struct eth_addr *mac, char *str);
int eth_addr_init(struct eth_addr *mac, const char *mac_str);

#endif

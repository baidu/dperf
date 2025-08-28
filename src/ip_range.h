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

#ifndef __IP_RANGE_H
#define __IP_RANGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <arpa/inet.h>

#include "ip.h"

struct ip_range {
    ipaddr_t start;
    uint32_t list[256];
    uint32_t valid[256];
    uint8_t num;
};

#define IP_RANGE_NUM_MAX    32
struct ip_group {
    struct ip_range ip_range[IP_RANGE_NUM_MAX];
    int num;
};

#define for_each_ip_range(ipg, ipr) \
    for (ipr = &((ipg)->ip_range[0]); (ipr - (&(ipg)->ip_range[0])) < (ipg)->num; ipr++)

static inline uint32_t ip_range_get(const struct ip_range *ip_range, uint8_t idx)
{
    idx = idx % ip_range->num;
    return ip_range->list[idx];
}

static inline void ip_range_get2(const struct ip_range *ip_range, uint8_t idx, ipaddr_t *addr)
{
    uint32_t last = ip_range_get(ip_range, idx);

    ipaddr_join(&ip_range->start, last, addr);
}

static inline int ip_range_init(struct ip_range *ip_range, ipaddr_t start, int num)
{
    int i = 0;
    uint32_t last_byte = ipaddr_last_byte(start);
    uint32_t ip = start.ip;
    uint32_t t = 0;

    /*
     * 1. the last byte cannot be 0 or 255, which are illegal unicast addresses.
     * 2. address cannot be 0.0.0.0
     */
    if ((ip == 0) || (num <= 0) || ((last_byte + num - 1) >= 255)) {
        return -1;
    }

    for (i = 0; i < num; i++) {
        t = htonl((ntohl(ip) + i));
        ip_range->list[i] = t;
        ip_range->valid[last_byte + i] = t;
    }
    ip_range->start = start;
    ip_range->num = num;
    return 0;
}

static inline int ip_range_add(struct ip_range *ip_range, ipaddr_t addr)
{
    int i = 0;
    uint32_t last = 0;

    if ((ip_range->num == 0) || (ip_range->num >= 255)) {
        return -1;
    }

    for (i = 0; i < 14; i++) {
        if (ip_range->start.byte[i] != addr.byte[i]) {
            return -1;
        }
    }

    last = addr.byte[15];
    if ((last == 0) || (last == 255)) {
        return -1;
    }
    if (ip_range->valid[last] == 0) {
        ip_range->list[ip_range->num] = addr.ip;
        ip_range->valid[last] = addr.ip;
        ip_range->num++;
        return 0;
    } else {
        return -1;
    }
}

static inline bool ip_range_exist(const struct ip_range *ip_range, uint32_t ip)
{
    return (ip == ip_range->valid[ntohl(ip) & 0xff]);
}

static inline bool ip_range_exist_ipv6(const struct ip_range *ip_range, const ipaddr_t *addr)
{
    /* 1. check the first 12 bytes */
    if (memcmp(&(addr->in6), &(ip_range->start), IP6_ADDR_SIZE - 4) != 0) {
        return false;
    }

    /* 1. check the last 4 bytes */
    return ip_range_exist(ip_range, addr->ip);
}

#endif

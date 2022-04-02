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
    uint32_t ip = ip_range->start.ip;

    idx = idx % ip_range->num;
    ip = ntohl(ip);
    ip += idx;
    return htonl(ip);
}

static inline void ip_range_get2(const struct ip_range *ip_range, uint8_t idx, ipaddr_t *addr)
{
    uint32_t last = ip_range_get(ip_range, idx);

    ipaddr_join(&ip_range->start, last, addr);
}

static inline int ip_range_init(struct ip_range *ip_range, ipaddr_t start, int num)
{
    uint32_t last_byte = ipaddr_last_byte(start);

    /*
     * 1. the last byte cannot be 0 or 255, which are illegal unicast addresses.
     * 2. address cannot be 0.0.0.0
     */
    if ((start.ip == 0) || (num <= 0) || ((last_byte + num - 1) >= 255)) {
        return -1;
    }

    ip_range->start = start;
    ip_range->num = num;
    return 0;
}

static inline bool ip_range_exist(const struct ip_range *ip_range, uint32_t ip)
{
    uint32_t base = ntohl(ip_range->start.ip);

    ip = ntohl(ip);
    return ((ip >= base) && (ip < (base + ip_range->num)));
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

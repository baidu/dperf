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

#ifndef __ICMP6_H_
#define __ICMP6_H_

#include <stdbool.h>
#include <rte_mbuf.h>
#include <netinet/icmp6.h>
#include "mbuf.h"

struct work_space;
void icmp6_process(struct work_space *ws, struct rte_mbuf *m);
void icmp6_ns_request(struct work_space *ws);

static inline bool icmp6_is_neigh(struct rte_mbuf *m)
{
    uint8_t type = 0;
    struct icmp6_hdr *icmp6h = NULL;

    icmp6h = mbuf_icmp6_hdr(m);
    type = icmp6h->icmp6_type;
    if ((type == ND_NEIGHBOR_SOLICIT) || (type == ND_NEIGHBOR_ADVERT)) {
        return true;
    }

    return false;
}

#endif

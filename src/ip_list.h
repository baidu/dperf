/*
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
 * Author: Jianzhang Peng (pengjianzhang@gmail.com)
 */

#ifndef __IP_LIST_H
#define __IP_LIST_H

#include "ip.h"

#define IP_LIST_NUM_MAX 65536

struct ip_list {
    int num;
    int next;
    int af;
    ipaddr_t ip[IP_LIST_NUM_MAX];
};

int ip_list_add(struct ip_list *ip_list, int af, ipaddr_t *ip);
int ip_list_split(struct ip_list *ip_list, struct ip_list *sub, int start, int step);

static inline void ip_list_get_next_ipv4(struct ip_list *ip_list, uint32_t *addr)
{
    *addr = ip_list->ip[ip_list->next].ip;
    ip_list->next++;
    if (unlikely(ip_list->next >= ip_list->num)) {
        ip_list->next = 0;
    }
}

static inline void ip_list_get_next_ipv6(struct ip_list *ip_list, struct in6_addr *addr)
{
    *addr = ip_list->ip[ip_list->next].in6;
    ip_list->next++;
    if (unlikely(ip_list->next >= ip_list->num)) {
        ip_list->next = 0;
    }
}

#endif

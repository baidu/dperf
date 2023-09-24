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

#include "ip_list.h"
#include <arpa/inet.h>

int ip_list_add(struct ip_list *ip_list, int af, ipaddr_t *ip)
{
    if ((ip_list == NULL) || (ip == NULL)) {
        return -1;
    }

    if ((af != AF_INET) && (af != AF_INET6)) {
        return -1;
    }

    if (ip_list->num >= IP_LIST_NUM_MAX) {
        return -1;
    }

    if (ip_list->num == 0) {
        ip_list->af = af;
    } else if (ip_list->af != af) {
        return -1;
    }

    ip_list->ip[ip_list->num] = *ip;
    ip_list->num++;

    return 0;
}

/*
 * return
 *  -1      error
 *  >= 0    number of elements in <sub>
 * */
int ip_list_split(struct ip_list *ip_list, struct ip_list *sub, int start, int step)
{
    int i = 0;

    if ((start < 0) || (step <= 0) || (ip_list == NULL) || (sub == NULL)) {
        return -1;
    }

    sub->num = 0;
    sub->next = 0;
    sub->af = ip_list->af;
    for (i = start; i < ip_list->num; i += step) {
        sub->ip[sub->num] = ip_list->ip[i];
        sub->num++;
    }

    return sub->num;
}

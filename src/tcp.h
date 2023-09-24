/*
 * Copyright (c) 2021-2022 Baidu.com, Inc. All Rights Reserved.
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
 * Author: Jianzhang Peng (pengjianzhang@baidu.com)
 *         Jianzhang Peng (pengjianzhang@gmail.com)
 */

#ifndef __TCP_H
#define __TCP_H

#include <stdint.h>
#include <netinet/ip.h>
#define __FAVOR_BSD
#include <netinet/tcp.h>
#undef __FAVOR_BSD
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_common.h>

struct work_space;
int tcp_init(struct work_space *ws);
void tcp_drop(__rte_unused struct work_space *ws, struct rte_mbuf *m);

#ifdef HTTP_PARSE
int tcp_ack_delay_flush(struct work_space *ws);
#else
#define tcp_ack_delay_flush(ws) do{}while(0)
#endif

#endif

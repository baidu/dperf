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

#include "socket_timer.h"

__thread struct socket_timer g_retransmit_timer;
__thread struct socket_timer g_keepalive_timer; /* client only */
__thread struct socket_timer g_timeout_timer;

void socket_timer_init(void)
{
    socket_queue_init(&g_retransmit_timer.queue);
    socket_queue_init(&g_keepalive_timer.queue);
    socket_queue_init(&g_timeout_timer.queue);
}

static inline void socket_timeout_handler(__rte_unused struct work_space *ws, struct socket *sk)
{
    net_stats_socket_error();
    socket_close(sk);
}

void socket_timeout_timer_expire(struct work_space *ws)
{
    uint64_t timeout_tsc = 0;
    struct socket_timer *timer = NULL;

    timer = &g_timeout_timer;
    timeout_tsc = (g_config.retransmit_timeout * RETRANSMIT_NUM_MAX) + g_config.keepalive_request_interval;
    socket_timer_run(ws, timer, timeout_tsc, socket_timeout_handler);
}

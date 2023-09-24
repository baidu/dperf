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

#ifndef __SOCKET_TIMER_H
#define __SOCKET_TIMER_H

#include "socket.h"
#include "work_space.h"

extern __thread struct socket_timer g_retransmit_timer;
extern __thread struct socket_timer g_keepalive_timer;
extern __thread struct socket_timer g_timeout_timer;

typedef void(*socket_timer_handler_t)(struct work_space *, struct socket *);

struct socket_queue {
    struct socket_node head;
};

struct socket_timer {
    struct socket_queue queue;
};

static inline void socket_queue_init(struct socket_queue *sq)
{
    socket_node_init(&sq->head);
}

static inline void socket_queue_del(struct socket *sk)
{
    struct socket_node *sn = &sk->node;
    socket_node_del(sn);
}

static inline void socket_queue_push(struct socket_queue *sq, struct socket *sk)
{
    struct socket_node *head = &sq->head;
    struct socket_node *sn = &(sk->node);
    struct socket_node *next = head;
    struct socket_node *prev = next->prev;

    /* [head->prev] [new-node] [head]  */
    sn->next = next;
    sn->prev = prev;
    next->prev = sn;
    prev->next = sn;
}

static inline struct socket *socket_queue_first(struct socket_queue *sq)
{
    struct socket_node *head = &sq->head;

    if (head->next != head) {
        return (struct socket *)(head->next);
    }

    return NULL;
}

static inline void socket_del_timer(struct socket *sk)
{
    socket_queue_del(sk);
}

static inline void socket_add_timer(struct socket_queue *head, struct socket *sk, uint64_t now_tsc)
{
    sk->timer_tsc = now_tsc;
    socket_queue_del(sk);
    socket_queue_push(head, sk);
}

static inline void socket_start_timeout_timer(struct socket *sk, uint64_t now_tsc)
{
    struct socket_queue *queue = &g_timeout_timer.queue;
    socket_add_timer(queue, sk, now_tsc);
}

static inline void socket_stop_timeout_timer(struct socket *sk)
{
    socket_queue_del(sk);
}

static inline uint64_t socket_accurate_timer_tsc(struct socket *sk, uint64_t now_tsc)
{
    uint64_t interval = g_config.keepalive_request_interval;
    /*
     * |-------------|-------------|-------------|
     * timer_tsc     |--|now_tsc
     * */
    if (((sk->timer_tsc + interval) < now_tsc) && ((sk->timer_tsc + 2 * interval) > now_tsc)) {
        now_tsc = sk->timer_tsc + interval;
    }

    return now_tsc;
}

static inline void socket_start_keepalive_timer(struct socket *sk, uint64_t now_tsc)
{
    struct socket_queue *queue = &g_keepalive_timer.queue;

    if (sk->keepalive && (sk->snd_nxt == sk->snd_una)) {
        now_tsc = socket_accurate_timer_tsc(sk, now_tsc);
        socket_add_timer(queue, sk, now_tsc);
    }
}

static inline void socket_start_retransmit_timer_force(struct socket *sk, uint64_t now_tsc)
{
    struct socket_queue *queue = &g_retransmit_timer.queue;
    socket_add_timer(queue, sk, now_tsc);
}

static inline void socket_start_retransmit_timer(struct socket *sk, uint64_t now_tsc)
{
    if (sk->snd_nxt != sk->snd_una) {
        socket_start_retransmit_timer_force(sk, now_tsc);
    }
}

static inline void socket_stop_retransmit_timer(__rte_unused struct socket *sk)
{
    sk->retrans = 0;
    if (sk->snd_nxt == sk->snd_una) {
        socket_del_timer(sk);
    }
}

static inline void socket_timer_run(struct work_space *ws, struct socket_timer *st, uint64_t timeout,
    socket_timer_handler_t handler)
{
    struct socket *sk = NULL;
    struct socket_queue *sq = &(st->queue);
    uint64_t now_tsc = work_space_tsc(ws);

    while ((sk = socket_queue_first(sq)) != NULL) {
        if (sk->timer_tsc + (timeout) <= now_tsc) {
            socket_del_timer(sk);
            handler(ws, sk);
        } else {
            break;
        }
    }
}

void socket_timer_init(void);
void socket_timeout_timer_expire(struct work_space *ws);

#endif

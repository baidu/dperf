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

#ifndef __TICK_H
#define __TICK_H

#include <stdint.h>
#include <sys/time.h>
#include <rte_cycles.h>

#define TICKS_PER_SEC_DEFAULT (10 * 1000)
#define TSC_PER_SEC g_tsc_per_second

extern uint64_t g_tsc_per_second;

struct tsc_time {
    uint64_t last;
    uint64_t count;
    uint64_t interval;
};

struct tick_time {
    uint64_t tsc;
    struct tsc_time tick;
    struct tsc_time ms100;
    struct tsc_time second;
};

static inline uint64_t tsc_time_go(struct tsc_time *tt, uint64_t tsc_now)
{
    uint64_t count = 0;

    while (tt->last + tt->interval <= tsc_now) {
        tt->count++;
        tt->last += tt->interval;
        count++;
    }

    return count;
}

static inline void tick_time_update(struct tick_time *tt)
{
    tt->tsc = rte_rdtsc();
}

void tick_init(int ticks_per_sec);
void tick_time_init(struct tick_time *tt);
void tick_wait_init(struct timeval *last_tv);
uint64_t tick_wait_one_second(struct timeval *last_tv);

#endif

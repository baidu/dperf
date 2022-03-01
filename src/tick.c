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

#include "tick.h"

#include <string.h>
#include <rte_cycles.h>

static uint64_t g_tsc_per_tick = 0;
uint64_t g_tsc_per_second = 0;

void tick_init(void)
{
    uint64_t hz = rte_get_tsc_hz();

    g_tsc_per_second = hz;
    g_tsc_per_tick = hz / TICKS_PER_SEC;
}

static void tsc_time_init(struct tsc_time *tt, uint64_t now, uint64_t interval)
{
    tt->last = now;
    tt->interval = interval;
    tt->count = 0;
}

void tick_time_init(struct tick_time *tt)
{
    uint64_t now = rte_rdtsc();

    memset(tt, 0, sizeof(struct tick_time));
    tsc_time_init(&tt->tick, now, g_tsc_per_tick);
    tsc_time_init(&tt->ms100, now, g_tsc_per_tick * 50);
    tsc_time_init(&tt->second, now, g_tsc_per_second);
}

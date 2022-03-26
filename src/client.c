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

#include "client.h"

#include <stdio.h>

#include "config.h"
#include "socket.h"
#include "tick.h"
#include "work_space.h"

static uint64_t client_assign_task(struct work_space *ws, uint64_t target)
{
    uint64_t val = 0;
    struct config *cfg = ws->cfg;
    uint64_t id = (uint64_t)(ws->id);
    uint64_t cpu_num = (uint64_t)cfg->cpu_num;

    /* low target. Some CPUs idle. Some CPUs have only 1 task*/
    if (target <= cpu_num) {
        if (id < target) {
            val = 1;
        }
    } else {
        val = (uint64_t)(((double)(target)) / cpu_num);
        if (id == 0) {
            val = target - val * (cpu_num - 1);
        }
    }

    return val;
}

int client_init(struct work_space *ws)
{
    uint64_t cps = 0;
    uint64_t cc = 0;
    struct client_launch *cl = &ws->client_launch;
    struct config *cfg = ws->cfg;

    cps = client_assign_task(ws, cfg->cps);
    cc = client_assign_task(ws, cfg->cc);

    /* This is an idle CPU */
    if (cps == 0) {
        return 0;
    }

    cl->cc = cc;
    /* For small scale test, launch once a second */
    if (cps <= cfg->launch_num) {
        cl->launch_num = cps;
        cl->launch_interval = g_tsc_per_second;
    } else {
        /* For large-scale tests, multiple launches in one second */
        cl->launch_num = cfg->launch_num;
        cl->launch_interval = (g_tsc_per_second / (cps / cl->launch_num));
    }
    cl->launch_interval_default = cl->launch_interval;
    cl->launch_next = rte_rdtsc() + g_tsc_per_second * cfg->wait;

    return 0;
}

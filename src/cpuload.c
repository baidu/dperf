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

#include "cpuload.h"

#include <stdint.h>
#include <string.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include "net_stats.h"

uint32_t cpuload_cal_cpusage(struct cpuload *load, uint64_t now_tsc)
{
    uint32_t usage = 0;
    uint64_t total = now_tsc - load->init_tsc;
    uint64_t work = load->work_tsc;

    if (work <= total) {
        /* reduce */
        work = work / 128;
        total = total / 128;
        /* avoid floating point operations */
        usage = (work * 100) / total;
    } else {
        usage = 100;
    }

    load->init_tsc = now_tsc;
    load->start_tsc = now_tsc;
    load->work_tsc = 0;

    return usage;
}

void cpuload_init(struct cpuload *load)
{
    memset(load, 0, sizeof(struct cpuload));
    load->init_tsc = rte_rdtsc();
}

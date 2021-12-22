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

#ifndef __CPULOAD_H
#define __CPULOAD_H

#include <stdint.h>

/*
 * Each thread calculates CPU usage every second.
 * cpu-usage = working-time / 1-seconds
 * */
struct cpuload {
    uint64_t init_tsc; /* updated every second */
    uint64_t start_tsc; /* update after accumulation */
    uint64_t work_tsc; /* accumulate in one second */
};

#define CPULOAD_ADD_TSC(load, now_tsc, work) do {           \
    if (work) {                                             \
        (load)->work_tsc += (now_tsc) - (load)->start_tsc;  \
    }                                                       \
    (load)->start_tsc = (now_tsc);                          \
    work = 0;                                               \
} while (0)                                                 \

void cpuload_init(struct cpuload *load);
uint32_t cpuload_cal_cpusage(struct cpuload *load, uint64_t now_tsc);

#endif

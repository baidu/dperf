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

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "config.h"
#include "net_stats.h"
#include "work_space.h"
#include "dpdk.h"
#include "ctl.h"
#include "neigh.h"

static int lcore_main(__rte_unused void *arg1)
{
    int id = 0;
    struct work_space *ws = NULL;

    id = rte_lcore_id();
    ws = work_space_new(&g_config, id);
    if (ws == NULL) {
        printf("work space init error\n");
        exit(-1);
    }
    port_clear(ws->port_id, ws->queue_id);

    if (neigh_check_gateway(ws) < 0) {
        printf("Error: bad gateway. dperf cannot find gateway's MAC address. Please check the link.\n");
        exit(-1);
    }

    ws->start = 1;
    ws->run_loop(ws);
    work_space_close(ws);

    return 0;
}

int main(int argc, char **argv)
{
    pthread_t thread;

    if (config_parse(argc, argv, &g_config) < 0) {
        return 1;
    }

    if (g_config.daemon) {
        if (daemon(1, 1) != 0) {
            printf("daemon error\n");
            return 1;
        }
    }

    if (ctl_thread_start(&g_config, &thread) < 0) {
        printf("ctl thread start error\n");
        return 1;
    }

    if (dpdk_init(&g_config, argv[0]) < 0) {
        printf("dpdk init fail\n");
        return 1;
    }

    dpdk_run(lcore_main, NULL);
    ctl_thread_wait(thread);
    dpdk_close(&g_config);

    return 0;
}

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

#include "dpdk.h"

#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_prefetch.h>
#include <rte_mbuf.h>

#include "mbuf.h"
#include "flow.h"
#include "tick.h"

static void dpdk_set_lcores(struct config *cfg, char *lcores)
{
    int i = 0;
    char lcore_buf[64];

    for (i = 0; i < cfg->cpu_num; i++) {
        if (i == 0) {
            sprintf(lcore_buf, "%d@(%d)", i, cfg->cpu[i]);
        } else {
            sprintf(lcore_buf, ",%d@(%d)", i, cfg->cpu[i]);
        }
        strcat(lcores, lcore_buf);
    }
}

static int dpdk_append_pci(struct config *cfg, int argc, char *argv[], char *flag_pci)
{
    int num = 0;
    struct netif_port *port = NULL;

    config_for_each_port(cfg, port) {
        if (strlen(port->pci) > 0) {
            argv[argc] = flag_pci;
            argv[argc+1] = port->pci;
            argc += 2;
            num += 2;
        }
    }

    return num;
}

static int dpdk_set_socket_mem(struct config *cfg, char *socket_mem, char *file_prefix)
{
    int size = 0;

    if (strlen(cfg->socket_mem) <= 0) {
        return 0;
    }

    size = snprintf(socket_mem, RTE_ARG_LEN, "--socket-mem=%s", cfg->socket_mem);
    if (size >= RTE_ARG_LEN) {
        return -1;
    }

    size = snprintf(file_prefix, RTE_ARG_LEN, "--file-prefix=%d", getpid());
    if (size >= RTE_ARG_LEN) {
        return -1;
    }

    return 0;
}

static int dpdk_eal_init(struct config *cfg, char *argv0)
{
    int argc = 4;
    char lcores[2048] = "--lcores=";
#if RTE_VERSION >= RTE_VERSION_NUM(20, 0, 0, 0)
    char flag_pci[] = "-a";
#else
    char flag_pci[] = "-w";
#endif
    char socket_mem[64] = "";
    char file_prefix[64] = "";
    char *argv[4 + (NETIF_PORT_MAX * 2)] = {argv0, lcores, socket_mem, file_prefix, NULL};

    if (dpdk_set_socket_mem(cfg, socket_mem, file_prefix) < 0) {
        printf("dpdk_set_socket_mem fail\n");
        return -1;
    }

    dpdk_set_lcores(cfg, lcores);
    argc += dpdk_append_pci(cfg, argc, argv, flag_pci);

    if (rte_eal_init(argc, argv) < 0) {
        printf("rte_eal_init fail\n");
        return -1;
    }

    return 0;
}

int dpdk_init(struct config *cfg, char *argv0)
{
    if (dpdk_eal_init(cfg, argv0) < 0) {
        printf("dpdk_eal_init fail\n");
        return -1;
    }

    if (port_init_all(cfg) < 0) {
        printf("port init fail\n");
        return -1;
    }

    if (port_start_all(cfg) < 0) {
        printf("start port fail\n");
        return -1;
    }

    if (flow_init(cfg) < 0) {
        printf("flow init fail\n");
        return -1;
    }

    tick_init();

    return 0;
}

void dpdk_close(struct config *cfg)
{
    flow_flush(cfg);
    port_stop_all(cfg);
}

void dpdk_run(int (*lcore_main)(void*), void* data)
{
    int lcore_id = 0;

    RTE_LCORE_FOREACH(lcore_id) {
        if (lcore_id == 0) {
            continue;
        }
        rte_eal_remote_launch(lcore_main, data, lcore_id);
    }

    lcore_main(data);
    rte_eal_mp_wait_lcore();
}

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

/*
 * control thread:
 *  1. write logs
 *  2. output statistics
 *  3. exit control
 *  4. signal handler
 * */

#include "ctl.h"

#include <stdio.h>
#include <signal.h>
#include <stdbool.h>

#include "work_space.h"
#include "net_stats.h"
#include "kni.h"

#define CTL_CLIENT_LOG LOG_DIR"/dperf-ctl-client.log"
#define CTL_SERVER_LOG LOG_DIR"/dperf-ctl-server.log"

static bool g_stop = false;

static FILE *ctl_log_open(struct config *cfg)
{
    if (cfg->daemon) {
        if (cfg->server) {
            return fopen(CTL_SERVER_LOG, "a");
        } else {
            return fopen(CTL_CLIENT_LOG, "a");
        }
    } else {
        return NULL;
    }
}

static void ctl_log_close(FILE *fp)
{
    if (fp != NULL) {
        fclose(fp);
    }
}

static struct timeval g_last_tv;
static void ctl_wait_init(void)
{
    tick_wait_init(&g_last_tv);
}

static void ctl_wait_1s(void)
{
    tick_wait_one_second(&g_last_tv);
}

static inline void ctl_clear_screen(FILE *fp)
{
    if (g_config.quiet || (g_config.clear_screen == false)) {
        return;
    }

    if (fp == NULL) {
        printf("\033[H\033[J");
    }
}

static inline void ctl_print_speed(FILE *fp, int *sec)
{
    ctl_wait_1s();
    ctl_clear_screen(fp);
    net_stats_print_speed(fp, (*sec)++);
}

static inline void ctl_print_total(FILE *fp)
{
    ctl_wait_1s();
    ctl_clear_screen(fp);
    net_stats_print_total(fp);
}

static void ctl_slow_start(FILE *fp, int *seconds)
{
    int i = 0;
    uint64_t launch_interval = 0;
    uint64_t cps = 0;
    uint64_t step = 0;
    int launch_num = g_config.launch_num;
    int slow_start = g_config.slow_start;

    step = (g_config.cps / g_config.cpu_num) / slow_start;
    if (step <= 0) {
        step = 1;
    }

    for (i = 1; i <= slow_start; i++) {
        cps = step * i;
        launch_interval = (g_tsc_per_second * launch_num) / cps;
        work_space_set_launch_interval(launch_interval);
        ctl_print_speed(fp, seconds);
        if (g_stop) {
            break;
        }
    }

    work_space_set_launch_interval(0);
}

static void *ctl_thread_main(void *data)
{
    int i = 0;
    int seconds = 0;
    int count = 0;
    FILE *fp = NULL;
    struct config *cfg = NULL;

    cfg = (struct config *)data;
    count = cfg->duration;
    cfg->duration += g_config.slow_start;

    fp = ctl_log_open(cfg);
    work_space_wait_start();
    kni_link_up(cfg);

    ctl_wait_init();
    /* slow start */
    if (cfg->server == 0) {
        ctl_slow_start(fp, &seconds);
    }

    for (i = 0; i < count; i++) {
        ctl_print_speed(fp, &seconds);
        if (g_stop) {
            break;
        }
    }

    work_space_stop_all();
    for (i = 0; i < DELAY_SEC; i++) {
        ctl_print_speed(fp, &seconds);
    }

    work_space_exit_all();
    ctl_print_total(fp);
    ctl_log_close(fp);

    return NULL;
}

static void ctl_signal_handler(int signum)
{
    if (signum == SIGINT) {
        g_stop = true;
    }
}

int ctl_thread_start(struct config *cfg, pthread_t *thread)
{
    int ret = 0;

    signal(SIGINT, ctl_signal_handler);

    ret = pthread_create(thread, NULL, ctl_thread_main, (void*)cfg);
    if (ret < 0) {
        return -1;
    }

    return 0;
}

void ctl_thread_wait(pthread_t thread)
{
    pthread_join(thread, NULL);
}

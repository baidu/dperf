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

#define CTL_LOG LOG_DIR"/dperf-ctl.log"
static bool g_stop = false;

static FILE *ctl_log_open(struct config *cfg)
{
    if (cfg->daemon) {
        return fopen(CTL_LOG, "a");
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

static void ctl_slow_start(FILE *fp, int *seconds)
{
    int i = 0;
    int times = 0;

    for (i = 0; i < SLOW_START_SEC; i++) {
        times = SLOW_START_SEC - i;
        work_space_client_launch_deceleration(times);
        sleep(1);
        net_stats_print_speed(fp, (*seconds)++);
    }

    work_space_client_launch_deceleration(0);
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

    fp = ctl_log_open(cfg);
    net_states_wait();

    /* slow start */
    if (cfg->server == 0) {
        ctl_slow_start(fp, &seconds);
    }

    for (i = 0; i < count; i++) {
        sleep(1);
        net_stats_print_speed(fp, seconds++);
        if (g_stop) {
            break;
        }
    }

    work_space_stop_all();
    for (i = 0; i < DELAY_SEC; i++) {
        sleep(1);
        net_stats_print_speed(fp, seconds++);
    }

    work_space_exit_all();
    sleep(1);
    net_stats_print_total(fp);
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

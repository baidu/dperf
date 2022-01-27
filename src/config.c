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

#include "config.h"

#include <stdio.h>
#include <getopt.h>
#include <dirent.h>

#include "client.h"
#include "config_keyword.h"
#include "http.h"
#include "ip_range.h"
#include "mbuf.h"
#include "port.h"
#include "socket.h"
#include "udp.h"
#include "version.h"

static int config_parse_daemon(int argc, char *argv[], void *data);
static int config_parse_keepalive(int argc, char *argv[], void *data);
static int config_parse_mode(int argc, char *argv[], void *data);
static int config_parse_cpu(int argc, char *argv[], void *data);
static int config_parse_socket_mem(int argc, char *argv[], void *data);
static int config_parse_port(int argc, char *argv[], void *data);
static int config_parse_duration(int argc, char *argv[], void *data);
static int config_parse_cps(int argc, char *argv[], void *data);
static int config_parse_cc(int argc, char *argv[], void *data);
static int config_parse_keepalive_request_interval(int argc, char *argv[], void *data);
static int config_parse_keepalive_request_num(int argc, char *argv[], void *data);
static int config_parse_launch_num(int argc, char *argv[], void *data);
static int config_parse_client(int argc, char *argv[], void *data);
static int config_parse_server(int argc, char *argv[], void *data);
static int config_parse_listen(int argc, char *argv[], void *data);
static int config_parse_payload_size(int argc, char *argv[], void *data);
static int config_parse_mss(int argc, char *argv[], void *data);
static int config_parse_synflood(int argc, char *argv[], void *data);
static int config_parse_protocol(int argc, char *argv[], void *data);
static int config_parse_tx_burst(int argc, char *argv[], void *data);
static int config_parse_slow_start(int argc, char *argv[], void *data);

#define _DEFAULT_STR(s) #s
#define DEFAULT_STR(s)  _DEFAULT_STR(s)

struct config g_config;
static struct config_keyword g_config_keywords[] = {
    {"daemon", config_parse_daemon, ""},
    {"keepalive", config_parse_keepalive, ""},
    {"mode", config_parse_mode, "client/server"},
    {"cpu", config_parse_cpu, "n0 n1 n2-n3..., eg 0-4 7 8 9 10"},
    {"socket_mem", config_parse_socket_mem, "n0,n1,n2..."},
    {"port", config_parse_port, "PCI IPAddress Gateway [Gateway-Mac], eg 0000:13:00.0 192.168.1.3 192.168.1.1"},
    {"duration", config_parse_duration, "Time, eg 1.5d, 2h, 3.5m, 100s, 100"},
    {"cps", config_parse_cps, "Number, eg 1m, 1.5m, 2k, 100"},
    {"cc", config_parse_cc, "Number, eg 100m, 1.5m, 2k, 100"},
    {"synflood", config_parse_synflood, ""},
    {"keepalive_request_interval", config_parse_keepalive_request_interval,
        "Time, eg 1ms, 1s, 60s, default " DEFAULT_STR(DEFAULT_INTERVAL) "s"},
    {"keepalive_request_num", config_parse_keepalive_request_num, "Number"},
    {"launch_num", config_parse_launch_num, "Number, default " DEFAULT_STR(DEFAULT_LAUNCH)},
    {"client", config_parse_client, "IPAddress Number"},
    {"server", config_parse_server, "IPAddress Number"},
    {"listen", config_parse_listen, "Port Number, default 80 1" },
    {"payload_size", config_parse_payload_size, "Number, 1-1400"},
    {"mss", config_parse_mss, "Number, default 1460"},
    {"protocol", config_parse_protocol, "tcp/udp, default tcp"},
    {"tx_burst", config_parse_tx_burst, "Number[1-1024]"},
    {"slow_start", config_parse_slow_start,
        "Number[" DEFAULT_STR(SLOW_START_MIN) "-" DEFAULT_STR(SLOW_START_MAX) "],"
        " default " DEFAULT_STR(SLOW_START_DEFAULT)},
    {NULL, NULL, NULL}
};

static struct option g_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"test", no_argument, NULL, 't'},
    {"conf", required_argument, NULL, 'c'},
    {"manual", no_argument, NULL, 'm'},
    {NULL, 0, NULL, 0}
};

static char *config_str_find_nondigit(char *s, bool float_enable)
{
    char *p = s;
    int point = 0;

    while (*p) {
        if ((*p >= '0') && (*p <= '9')) {
            p++;
            continue;
        } else if (float_enable && (*p == '.')){
            p++;
            point++;
            if (point > 1) {
                return NULL;
            }
        } else {
            return p;
        }
    }

    return NULL;
}

static int config_parse_number(char *str, bool float_enable, bool rate_enable)
{
    char *p = NULL;
    int rate = 1;
    int val = 0;

    p = config_str_find_nondigit(str, float_enable);
    if (p != NULL) {
        if (rate_enable == false) {
            return -1;
        }

        if (strlen(p) != 1) {
            return -1;
        }

        if ((*p == 'k') || (*p == 'K')) {
            rate = 1000;
        } else if ((*p == 'm') || (*p == 'M')) {
            rate = 1000000;
        } else {
            return -1;
        }
    }

    if (p == str) {
        return -1;
    }

    if (float_enable) {
        val = atof(str) * rate;
    } else {
        val = atoi(str) * rate;
    }

    if (val < 0) {
        return -1;
    }

    return val;
}

static int config_parse_daemon(__rte_unused int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    cfg->daemon = 1;
    return 0;
}

static int config_parse_keepalive(__rte_unused int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    cfg->keepalive = 1;
    return 0;
}

static int config_parse_mode(int argc, char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (strcmp("client", argv[1]) == 0) {
        cfg->server = 0;
    } else if (strcmp("server", argv[1]) == 0) {
        cfg->server = 1;
    } else {
        printf("unknown mode %s\n", argv[1]);
        return -1;
    }

    return 0;
}

static int config_parse_cpu(int argc, char *argv[], void *data)
{
    int i = 0;
    int cpu = 0;
    int cpu_num = 0;
    int cpu_min = 0;
    int cpu_max = 0;
    char *p = NULL;
    struct config *cfg = data;

    if (argc <= 1) {
        return -1;
    }

    for (i = 1; i < argc; i++) {
        p = argv[i];
        cpu_min = -1;
        cpu_max = -1;
        if (strchr(p, '-') != NULL) {
            if (sscanf(p,"%d-%d", &cpu_min, &cpu_max) != 2) {
                return -1;
            }
        } else {
            cpu_min = cpu_max = config_parse_number(p, false, false);
        }

        if ((cpu_min < 0) || (cpu_max < 0) || (cpu_min > cpu_max)) {
            printf("bad cpu number %s\n", p);
            return -1;
        }

        for (cpu = cpu_min; cpu <= cpu_max; cpu++) {
            if (cpu_num + 1 > THREAD_NUM_MAX) {
                printf("too much cpu %d > %d\n", cpu_num + 1, THREAD_NUM_MAX);
            }
            cfg->cpu[cpu_num] = cpu;
            cpu_num++;
        }
    }
    cfg->cpu_num = cpu_num;

    return 0;
}

static int config_parse_socket_mem(int argc, char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (strlen(argv[1]) >= RTE_ARG_LEN) {
        return -1;
    }

    strcpy(cfg->socket_mem, argv[1]);

    return 0;
}

static int config_parse_ip(struct config *cfg, const char *str, ipaddr_t *ip)
{
    int af = ipaddr_init(ip, str);

    if ((af > 0) && ((cfg->af == 0) || (cfg->af == af))) {
        cfg->af = af;
        cfg->ipv6 = (af == AF_INET6);
        return 0;
    } else {
        return -1;
    }
}

static int config_parse_port(int argc, char *argv[], void *data)
{
    struct netif_port *port = NULL;
    struct config *cfg = data;

    if ((argc < 4) || (argc > 5)) {
        return -1;
    }

    if (cfg->port_num >= NETIF_PORT_MAX) {
        return -1;
    }
    port = &cfg->ports[cfg->port_num];
    if (strlen(argv[1]) != PCI_LEN) {
        return -1;
    }
    strcpy(port->pci, argv[1]);

    if (config_parse_ip(cfg, argv[2], &port->local_ip) < 0) {
        return -1;
    }

    if (config_parse_ip(cfg, argv[3], &port->gateway_ip) < 0) {
        return -1;
    }

    if (argc == 5) {
        if (eth_addr_init(&port->gateway_mac, argv[4]) != 0) {
            return -1;
        }
    }

    port->id = -1;
    cfg->port_num++;
    return 0;
}

static int config_parse_listen(int argc, char *argv[], void *data)
{
    int listen = 0;
    int listen_num = 0;
    struct config *cfg = data;

    if (argc != 3) {
        return -1;
    }

    listen = config_parse_number(argv[1], false, false);
    listen_num = config_parse_number(argv[2], false, false);
    if ((listen <= 0) || (listen_num <= 0)) {
        return -1;
    }

    if ((listen + listen_num) >= (NETWORK_PORT_NUM - 1)) {
        return -1;
    }

    cfg->listen = listen;
    cfg->listen_num = listen_num;

    return 0;
}

static int config_parse_ip_range(struct config *cfg, int argc, char *argv[], struct ip_range *ip_range)
{
    int num = 0;
    ipaddr_t ip;

    if (argc != 3) {
        return -1;
    }

    if (config_parse_ip(cfg, argv[1], &ip) < 0) {
        return -1;
    }

    if (ip.ip == 0) {
        return -1;
    }

    num = config_parse_number(argv[2], false, false);
    if (ip_range_init(ip_range, ip, num) < 0) {
        printf("bad client ip range\n");
        return -1;
    }

    return 0;
}

static int config_parse_ip_group(struct config *cfg, int argc, char *argv[], struct ip_group *ip_group)
{
    int ret = 0;
    struct ip_range *ip_range = NULL;

    if (ip_group->num >= IP_RANGE_NUM_MAX) {
        return -1;
    }

    ip_range = &ip_group->ip_range[ip_group->num];
    ret = config_parse_ip_range(cfg, argc, argv, ip_range);
    if (ret == 0) {
        ip_group->num++;
    }

    return ret;
}

static int config_parse_client(int argc, char *argv[], void *data)
{
    struct config *cfg = data;
    struct ip_group *ip_group = &cfg->client_ip_group;

    return config_parse_ip_group(cfg, argc, argv, ip_group);
}

static int config_parse_server(int argc, char *argv[], void *data)
{
    struct config *cfg = data;
    struct ip_group *ip_group = &cfg->server_ip_group;

    return config_parse_ip_group(cfg, argc, argv, ip_group);
}

static int config_parse_duration(int argc, char *argv[], void *data)
{
    int c = 0;
    int len = 0;
    int rate = 1;
    int duration = 0;
    double val = 0.0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    len = strlen(argv[1]);
    c = argv[1][len -1];
    if (c == 'm') {
        rate = 60;
    } else if (c == 'h') {
        rate = 60 * 60;
    } else if (c == 'd') {
        rate = 60 * 60 * 24;
    }

    val = atof(argv[1]);
    if (val < 0) {
        return -1;
    }
    duration = val * rate;
    if (duration <= 0) {
        return -1;
    }

    cfg->duration = duration;
    return 0;
}

static int config_parse_cps(int argc, char *argv[], void *data)
{
    int cps = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    cps = config_parse_number(argv[1], true, true);
    if (cps < 0) {
        return -1;
    }

    cfg->cps = cps;
    return 0;
}

static int config_parse_cc(int argc, char *argv[], void *data)
{
    int cc = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    cc = config_parse_number(argv[1], true, true);
    if (cc <= 0) {
        return -1;
    }

    cfg->cc = cc;
    return 0;
}

static int config_parse_synflood(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 1) {
        return -1;
    }

    cfg->synflood = 1;
    return 0;
}

static int config_parse_keepalive_request_interval(int argc, char *argv[], void *data)
{
    char *p = NULL;
    int val = 0;
    int rate = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    p = config_str_find_nondigit(argv[1], false);
    if (p == NULL) {
        return -1;
    }

    if (strcmp(p, "ms") == 0) {
        rate = TICKS_PER_SEC / 1000;
    } else if (strcmp(p, "s") == 0) {
        rate = TICKS_PER_SEC;
    } else {
        return -1;
    }

    val = atoi(argv[1]);
    if (val <= 0) {
        return -1;
    }

    cfg->keepalive_request_interval = val * rate;
    return 0;
}

static int config_parse_keepalive_request_num(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = config_parse_number(argv[1], true, true);
    if (val < 0) {
        return -1;
    }
    cfg->keepalive_request_num = val;
    return 0;
}

static int config_parse_launch_num(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = config_parse_number(argv[1], false, false);
    if (val < 0) {
        return -1;
    }
    cfg->launch_num = val;
    return 0;
}

static inline int config_parse_tx_burst(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = config_parse_number(argv[1], false, false);
    if ((val < 1) || (val > TX_BURST_MAX)) {
        return -1;
    }
    cfg->tx_burst = val;
    return 0;
}

static int config_parse_slow_start(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = config_parse_number(argv[1], false, false);
    if ((val < SLOW_START_MIN) || (val > SLOW_START_MAX)) {
        return -1;
    }
    cfg->slow_start = val;
    return 0;
}

static int config_parse_payload_size(int argc, char *argv[], void *data)
{
    int payload_size = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    payload_size = config_parse_number(argv[1], true, true);
    if ((payload_size < 0) || (payload_size > PAYLOAD_SIZE_MAX)) {
        return -1;
    }

    cfg->payload_size = payload_size;
    return 0;
}

static int config_parse_mss(int argc, char *argv[], void *data)
{
    int mss = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    mss = config_parse_number(argv[1], false, false);
    if ((mss < 0) || (mss > MSS_MAX)) {
        return -1;
    }

    cfg->mss = mss;
    return 0;
}

static int config_parse_protocol(int argc, char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (strcmp(argv[1], "tcp") == 0) {
        cfg->protocol = IPPROTO_TCP;
        return 0;
    } else if (strcmp(argv[1], "udp") == 0) {
        cfg->protocol = IPPROTO_UDP;
        return 0;
    } else {
        return -1;
    }
}

static void config_manual(void)
{
    config_keyword_help(g_config_keywords);
}

static void config_help(void)
{
    printf("-h --help\n");
    printf("-v --version\n");
    printf("-t --test       Test configure file and exit\n");
    printf("-c --conf file  Run with conf file\n");
    printf("-m --manual     Show manual\n");
}

static void version(void)
{
    printf("%s\n", VERSION);
}

static int config_port_queue_num(struct config *cfg)
{
    return cfg->cpu_num / cfg->port_num;
}

struct netif_port *config_port_get(struct config *cfg, int thread_id, int *p_queue_id)
{
    int queue_num = config_port_queue_num(cfg);
    int queue_id = thread_id % queue_num;
    int port_idx = thread_id / queue_num;

    if (p_queue_id) {
        *p_queue_id = queue_id;
    }

    return &cfg->ports[port_idx];
}

static int config_set_port_ip_range(struct config *cfg)
{
    int i = 0;
    struct netif_port *port = NULL;

    if (cfg->port_num == 0) {
        printf("no port\n");
        return -1;
    }

    if (cfg->client_ip_group.num == 0) {
        printf("no client_ip_range\n");
        return -1;
    }

    if (cfg->server_ip_group.num == 0) {
        printf("no server_ip_range\n");
        return -1;
    }

    if (cfg->port_num != cfg->server_ip_group.num) {
        printf("port num (%d) != server_ip_range (%d)\n", cfg->port_num, cfg->server_ip_group.num);
        return -1;
    }

    if (cfg->server == 0) {
        if (cfg->client_ip_group.num != cfg->port_num) {
            printf("port num (%d) != client_ip_range (%d)\n", cfg->port_num, cfg->client_ip_group.num);
            return -1;
        }
    }

    config_for_each_port(cfg, port) {
        if (cfg->server == 0) {
            port->local_ip_range = &cfg->client_ip_group.ip_range[i];
        } else {
            port->local_ip_range = &cfg->server_ip_group.ip_range[i];
        }

        port->client_ip_range = cfg->client_ip_group.ip_range[i];
        port->server_ip_range = cfg->server_ip_group.ip_range[i];
        if (port->queue_num != port->server_ip_range.num) {
            printf("queue num not equal server ip num\n");
            return -1;
        }
        i++;
    }

    return 0;
}

static int config_check_port(struct config *cfg)
{
    struct netif_port *port = NULL;
    struct netif_port *port0 = NULL;
    struct netif_port *port1 = NULL;

    if (cfg->cpu_num == 0) {
        printf("not found cpu\n");
        return -1;
    }

    if (cfg->port_num == 0) {
        printf("no found port\n");
        return -1;
    }

    if ((cfg->cpu_num % cfg->port_num) != 0) {
        printf("the number of CPUs(%d) is not a multiple of the number of ports(%d)\n", cfg->cpu_num, cfg->port_num);
        return -1;
    }

    config_for_each_port(cfg, port0) {
        config_for_each_port(cfg, port1) {
            if (port0 == port1) {
                continue;
            }

            if (strcmp(port0->pci, port1->pci) == 0) {
                printf("duplicate pci %s\n", port0->pci);
                return -1;
            }
        }
    }

    config_for_each_port(cfg, port) {
        port->queue_num = config_port_queue_num(cfg);
    }

    return 0;
}

static int config_check_client_addr(const struct config *cfg)
{
    int i = 0;
    int ret = 0;
    uint8_t *client_ip = NULL;
    uint32_t low_2byte = 0;
    const struct ip_range *ip_range = NULL;

    client_ip = calloc(0xffff, 1);
    if (client_ip == NULL) {
        printf("check client addr: memory alloc fail\n");
        return -1;
    }

    for_each_ip_range(&cfg->client_ip_group, ip_range) {
        low_2byte = ip_range_get(ip_range, 0);
        low_2byte = ntohl(low_2byte) & 0xffff;

        for (i = 0; i < ip_range->num; i++) {
            if (client_ip[low_2byte + i] == 0) {
                client_ip[low_2byte + i] = 1;
            } else {
                ret = -1;
                printf("duplicate client ip address's last 2 byte\n");
                goto out;
            }
        }
    }

out:
    if (client_ip) {
        free(client_ip);
    }
    return ret;
}

static int config_check_local_addr(const struct config *cfg)
{
   const struct netif_port *port0 = NULL;
   const struct netif_port *port1 = NULL;

    config_for_each_port(cfg, port0) {
        config_for_each_port(cfg, port1) {
            if (port0 == port1) {
                continue;
            }

            if (ipaddr_eq(&port0->local_ip, &port1->local_ip)) {
                printf("duplicate port ip\n");
                return -1;
            }
        }
    }

    return 0;
}

static bool config_ip_range_overlap(const struct ip_range *ip_range0, const struct ip_range *ip_range1)
{
    int i = 0;
    int j = 0;
    ipaddr_t ip0;
    ipaddr_t ip1;

    for (i = 0; i < ip_range0->num; i++) {
        for (j = 0; j < ip_range1->num; j++) {
            ip_range_get2(ip_range0, i, &ip0);
            ip_range_get2(ip_range1, j, &ip1);
            if (ipaddr_eq(&ip0, &ip1)) {
                return true;
            }
        }
    }

    return false;
}

static int config_check_logdir(const struct config *cfg)
{
    int log = 0;
    DIR *dir = NULL;

    if (cfg->daemon) {
        log = 1;
    }

#ifdef DPERF_DEBUG
        log = 1;
#endif

    if (log) {
        dir = opendir(LOG_DIR);
        if (dir == NULL) {
            printf("%s not exist\n", LOG_DIR);
            return -1;
        }

        closedir(dir);
    }

    return 0;
}

static int config_check_server_addr(const struct config *cfg)
{
    const struct ip_range *ip_range0 = NULL;
    const struct ip_range *ip_range1 = NULL;

    for_each_ip_range(&(cfg->server_ip_group), ip_range0) {
        for_each_ip_range(&(cfg->server_ip_group), ip_range1) {
            if (ip_range0 == ip_range1) {
                continue;
            }

            if (config_ip_range_overlap(ip_range0, ip_range1)) {
                printf("duplicate server ip address\n");
                return -1;
            }
        }
    }

    return 0;
}

static int config_check_slow_start(struct config *cfg)
{

    if (cfg->server) {
        if (cfg->slow_start != 0) {
            printf("slow_start in server config\n");
            return -1;
        }
        return 0;
    }

    if (cfg->slow_start == 0) {
        cfg->slow_start = SLOW_START_DEFAULT;
    }

    return 0;
}

static int config_check_target(struct config *cfg)
{
    int i = 0;
    uint32_t socket_num = 0;
    uint64_t cc = 0;
    uint64_t cps = 0;
    uint64_t cps_cc = 0;

    cps = cfg->cps / cfg->cpu_num;
    cps_cc = cps * RETRANSMIT_TIMEOUT_SEC;
    cc = cfg->cc / cfg->cpu_num;
    for (i = 0; i < cfg->cpu_num; i++) {
        socket_num = config_get_total_socket_num(cfg, i);
        if (socket_num < cc) {
            printf("Error: insufficient sockets. worker=%d sockets=%u cc=%lu\n", i, socket_num, cc);
            return -1;
        }

        if (socket_num < cps_cc) {
            printf("Error: insufficient sockets. worker=%d sockets=%u cps's cc=%lu\n", i, socket_num, cps_cc);
            return -1;
        }
    }

    return 0;
}

int config_parse(int argc, char **argv, struct config *cfg)
{
    int opt = 0;
    int test = 0;
    const char *optstr = "hvtmc:";

    if (argc == 1) {
        config_help();
        return -1;
    }

    memset(cfg, 0, sizeof(struct config));
    while ((opt = getopt_long_only(argc, argv, optstr, g_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                if (config_keyword_parse(optarg, g_config_keywords, cfg) < 0) {
                    return -1;
                }
                break;
            case 'h':
                config_help();
                exit(0);
                break;
            case 'v':
                version();
                exit(0);
                break;
            case 't':
                test = 1;
                break;
            case 'm':
                config_manual();
                exit(0);
                break;
            default:
                return -1;
        }
    }

    if (cfg->protocol == 0) {
        cfg->protocol = IPPROTO_TCP;
    }

    if (cfg->duration == 0) {
        cfg->duration = DEFAULT_DURATION;
    }

    if (cfg->mss == 0) {
        cfg->mss = MSS_MAX;
    }

    if ((cfg->listen == 0) || (cfg->listen_num == 0)) {
        cfg->listen = 80;
        cfg->listen_num = 1;
    }

    if (cfg->server == 0) {
        if ((cfg->cps == 0) && (cfg->cc == 0)) {
            printf("no targets set\n");
            return -1;
        }

        if ((cfg->cc > 0) && (cfg->synflood == true)) {
            printf("'cc' conflicts with 'synflood'\n");
            return -1;
        }

        if (cfg->cc) {
            cfg->keepalive = 1;
            if (cfg->keepalive_request_interval == 0) {
                cfg->keepalive_request_interval = DEFAULT_INTERVAL * TICKS_PER_SEC;
            }

            if (cfg->cps == 0) {
                cfg->cps = DEFAULT_CPS;
            }
        } else {
            cfg->keepalive = 0;
            cfg->keepalive_request_interval = 0;
        }

    } else {
        cfg->cc = 0;
        cfg->cps = 0;
        cfg->synflood = 0;
    }

    if (cfg->launch_num == 0) {
        cfg->launch_num = DEFAULT_LAUNCH;
    }

    if (config_check_slow_start(cfg) < 0) {
        return -1;
    }

    if (config_check_client_addr(cfg) < 0) {
        return -1;
    }

    if (config_check_server_addr(cfg) < 0) {
        return -1;
    }

    if (config_check_local_addr(cfg) < 0) {
        return -1;
    }

    http_set_payload(cfg->payload_size);
    udp_set_payload(cfg->payload_size);
    if (config_check_port(cfg) != 0) {
        return -1;
    }

    if (config_set_port_ip_range(cfg) != 0) {
        return -1;
    }

    if (cfg->tx_burst == 0) {
        cfg->tx_burst = TX_BURST_DEFAULT;
    }

    if (config_check_logdir(cfg) < 0) {
        return -1;
    }

    if (config_check_target(cfg) < 0) {
        return -1;
    }

    if (test) {
        printf("Config file OK\n");
        exit(0);
    }

    return 0;
}

static uint32_t config_client_ip_range_socket_num(struct config *cfg, struct ip_range *ip_range)
{
    /*
     * client-ip-num * client-port-num * server-ip-num * server-listen-port-num
     * client-ip-num: 1-65535, skip port 0
     * server-ip-num: 1, each thread using one server-ip
     * */
    return ip_range->num * cfg->listen_num * (NETWORK_PORT_NUM - 1);
}

uint32_t config_get_total_socket_num(struct config *cfg, int id)
{
    uint32_t num = 0;
    struct ip_range *client_ip_range = NULL;
    struct netif_port *port = NULL;

    port = config_port_get(cfg, id, NULL);
    if (cfg->server) {
        /*
         * the DUT(eg load balancer) may connect to all servers
         * */
        for_each_ip_range(&cfg->client_ip_group, client_ip_range) {
            num += config_client_ip_range_socket_num(cfg, client_ip_range);
        }
    } else {
        client_ip_range = &(port->client_ip_range);
        num = config_client_ip_range_socket_num(cfg, client_ip_range);
    }

    return num;
}

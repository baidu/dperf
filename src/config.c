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
#include <string.h>
#include <ctype.h>

#include "client.h"
#include "config_keyword.h"
#include "http.h"
#include "ip_range.h"
#include "ip_list.h"
#include "mbuf.h"
#include "port.h"
#include "socket.h"
#include "udp.h"
#include "version.h"
#include "vxlan.h"
#include "bond.h"
#include "kni.h"

static int config_parse_daemon(int argc, char *argv[], void *data);
static int config_parse_keepalive(int argc, char *argv[], void *data);
static int config_parse_pipeline(int argc, char *argv[], void *data);
static int config_parse_mode(int argc, char *argv[], void *data);
static int config_parse_cpu(int argc, char *argv[], void *data);
static int config_parse_socket_mem(int argc, char *argv[], void *data);
static int config_parse_port(int argc, char *argv[], void *data);
static int config_parse_duration(int argc, char *argv[], void *data);
static int config_parse_cps(int argc, char *argv[], void *data);
static int config_parse_cc(int argc, char *argv[], void *data);
static int config_parse_launch_num(int argc, char *argv[], void *data);
static int config_parse_client(int argc, char *argv[], void *data);
static int config_parse_server(int argc, char *argv[], void *data);
static int config_parse_change_dip(int argc, char *argv[], void *data);
static int config_parse_listen(int argc, char *argv[], void *data);
static int config_parse_payload_random(int argc, char *argv[], void *data);
static int config_parse_payload_size(int argc, char *argv[], void *data);
static int config_parse_packet_size(int argc, char *argv[], void *data);
static int config_parse_mss(int argc, char *argv[], void *data);
static int config_parse_flood(int argc, char *argv[], void *data);
static int config_parse_protocol(int argc, char *argv[], void *data);
static int config_parse_tx_burst(int argc, char *argv[], void *data);
static int config_parse_slow_start(int argc, char *argv[], void *data);
static int config_parse_wait(int argc, char *argv[], void *data);
static int config_parse_vxlan(int argc, char *argv[], void *data);
static int config_parse_vlan(int argc, char *argv[], void *data);
static int config_parse_kni(int argc, char *argv[], void *data);
static int config_parse_tos(int argc, char *argv[], void *data);
static int config_parse_jumbo(int argc, char *argv[], void *data);
static int config_parse_rss(int argc, char *argv[], void *data);
static int config_parse_quiet(int argc, char *argv[], void *data);
static int config_parse_tcp_rst(int argc, char *argv[], void *data);
static int config_parse_http_host(int argc, char *argv[], void *data);
static int config_parse_http_path(int argc, char *argv[], void *data);
static int config_parse_lport_range(int argc, char *argv[], void *data);
static int config_parse_client_hop(int argc, char *argv[], void *data);

#define _DEFAULT_STR(s) #s
#define DEFAULT_STR(s)  _DEFAULT_STR(s)

struct config g_config = {
    .tcp_rst = true,
};
static struct config_keyword g_config_keywords[] = {
    {"daemon", config_parse_daemon, ""},
    {"keepalive", config_parse_keepalive, "Interval(Timeout) [Number[0-" DEFAULT_STR(KEEPALIVE_REQ_NUM) "]], "
                "eg 1ms/10us/1s"},
    {"pipeline", config_parse_pipeline, "Number[" DEFAULT_STR(PIPELINE_MIN) "-" DEFAULT_STR(PIPELINE_MAX)"], default " DEFAULT_STR(PIPELINE_DEFAULT)},
    {"mode", config_parse_mode, "client/server"},
    {"cpu", config_parse_cpu, "n0 n1 n2-n3..., eg 0-4 7 8 9 10"},
    {"socket_mem", config_parse_socket_mem, "n0,n1,n2..."},
    {"port", config_parse_port, "PCI/bondMode:Policy(PCI0,PCI1,...) IPAddress Gateway [Gateway-Mac], eg 0000:13:00.0 192.168.1.3 192.168.1.1"},
    {"duration", config_parse_duration, "Time, eg 1.5d, 2h, 3.5m, 100s, 100"},
    {"cps", config_parse_cps, "Number, eg 1m, 1.5m, 2k, 100"},
    {"cc", config_parse_cc, "Number, eg 100m, 1.5m, 2k, 100"},
    {"flood", config_parse_flood, ""},
    {"launch_num", config_parse_launch_num, "Number, default " DEFAULT_STR(DEFAULT_LAUNCH)},
    {"client", config_parse_client, "IPAddress Number"},
    {"server", config_parse_server, "IPAddress Number"},
    {"change_dip", config_parse_change_dip, "IPAddress Step Number"},
    {"listen", config_parse_listen, "Port Number, default 80 1" },
    {"payload_random", config_parse_payload_random, ""},
    {"payload_size", config_parse_payload_size, "Number"},
    {"packet_size", config_parse_packet_size, "Number"},
    {"mss", config_parse_mss, "Number, default 1460"},
    {"protocol", config_parse_protocol, "http/tcp/udp, default tcp"},
    {"tx_burst", config_parse_tx_burst, "Number[1-1024]"},
    {"slow_start", config_parse_slow_start,
        "Number[" DEFAULT_STR(SLOW_START_MIN) "-" DEFAULT_STR(SLOW_START_MAX) "],"
        " default " DEFAULT_STR(SLOW_START_DEFAULT)},
    {"wait", config_parse_wait, "Number, default " DEFAULT_STR(WAIT_DEFAULT)},
    {"vxlan", config_parse_vxlan, "vni inner-smac inner-dmac vtep-local num vtep-remote num"},
    {"vlan", config_parse_vlan, "vlanID[" DEFAULT_STR(VLAN_ID_MIN) "-" DEFAULT_STR(VLAN_ID_MAX) "]"},
    {"kni", config_parse_kni, "[ifName], default " KNI_NAME_DEFAULT},
    {"tos", config_parse_tos, "Number[0x00-0xff], default 0, eg 0x01 or 1"},
    {"jumbo", config_parse_jumbo, ""},
    {"rss", config_parse_rss, "[l3/l3l4/auto [mq_rx_none|mq_rx_rss|l3|l3l4], default l3 mq_rx_rss"},
    {"quiet", config_parse_quiet, ""},
    {"tcp_rst", config_parse_tcp_rst, "Number[0-1], default 1"},
    {"http_host", config_parse_http_host, "String, default " HTTP_HOST_DEFAULT},
    {"http_path", config_parse_http_path, "String, default " HTTP_PATH_DEFAULT},
    {"lport_range", config_parse_lport_range, "Number [Number], default 1 65535"},
    {"client_hop", config_parse_client_hop, ""},
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

static int config_parse_keepalive_request_interval(struct config *cfg, char *str)
{
    char *p = NULL;
    int val = 0;
    int rate = 0;

    p = config_str_find_nondigit(str, false);
    if (p == NULL) {
        return -1;
    }

    if (strcmp(p, "us") == 0) {
        rate = 1;
    } else if (strcmp(p, "ms") == 0) {
        rate = 1000;
    } else if (strcmp(p, "s") == 0) {
        rate = 1000 * 1000; /* ms */
    } else {
        return -1;
    }

    val = atoi(str);
    if (val < 0) {
        return -1;
    }

    if (rate == 1) {
        if ((val >= 10) && (val % 10) != 0) {
            printf("Error: keepalive request interval must be a multiple of 10us\n");
            return -1;
        } else if (((val > 1)) && (val < 10) && (val % 2) != 0) {
            printf("Error: keepalive request interval must be a multiple of 2us\n");
            return -1;
        }

        if ((val >= 1000) && ((val % 1000) != 0)) {
            printf("Error: microseconds can only be used if the interval is less than 1 millisecond\n");
            return -1;
        }
    }

    val *= rate;
    cfg->keepalive_request_interval_us = val;
    return 0;
}

static int config_parse_keepalive_request_num(struct config *cfg, char *str)
{
    int val = 0;

    val = config_parse_number(str, true, true);
    if (val < 0) {
        return -1;
    }

    if (val > KEEPALIVE_REQ_NUM) {
        return -1;
    }

    cfg->keepalive_request_num = val;
    return 0;
}

static int config_parse_keepalive(int argc, char *argv[], void *data)
{
    struct config *cfg = data;

    if ((argc > 3) || (argc < 2)) {
        return -1;
    }

    if (cfg->keepalive) {
        printf("duplicate \'keepalive\'\n");
        return -1;
    }

    if (config_parse_keepalive_request_interval(cfg, argv[1]) < 0) {
        return -1;
    }

    if (argc == 3) {
        if (config_parse_keepalive_request_num(cfg, argv[2]) < 0) {
            return -1;
        }
    }

    if ((cfg->keepalive_request_interval_us == 0) && (cfg->keepalive_request_num != 0)) {
        return -1;
    }

    cfg->keepalive = true;
    return 0;
}

static int config_parse_pipeline(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if ((val = atoi(argv[1])) < 0) {
        return -1;
    }

    if ((val < PIPELINE_MIN) || (val > PIPELINE_MAX)) {
        return -1;
    }

    cfg->pipeline = (uint8_t)val;
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
    int j = 0;
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

    /* unique */
    for (i = 0; i < cpu_num; i++) {
        for (j = i + 1; j < cpu_num; j++) {
            if (cfg->cpu[i] == cfg->cpu[j]) {
                return -1;
            }
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

static int config_parse_ip(const char *str, ipaddr_t *ip)
{
    return ipaddr_init(ip, str);
}

static int config_set_af(struct config *cfg, int af)
{
    if ((af > 0) && ((cfg->af == 0) || (cfg->af == af))) {
        cfg->af = af;
        cfg->ipv6 = (af == AF_INET6);
        return 0;
    } else {
        return -1;
    }
}

#define BOND_STR_BASE   9
#define BOND_STR_MIN    (BOND_STR_BASE + PCI_LEN)
#define BOND_STR_MAX    (BOND_STR_BASE + ((PCI_LEN + 1) * PCI_NUM_MAX) - 1)
#define BOND_NUM(len)   (((len) - BOND_STR_BASE + 1) / (PCI_LEN + 1))

/*
 * example:
 * bond1:0(0000:81:10.0,0000:81:10.0,0000:81:10.0)
 * */
static int config_parse_bond(struct netif_port *port, char *str)
{
    uint8_t policy = 0;
    uint8_t mode = 0;
    int str_len = 0;
    int pci_num = 0;
    int i = 0;
    int j = 0;
    char *p = NULL;

    str_len = strlen(str);
    if ((str_len < BOND_STR_MIN) || (str_len > BOND_STR_MAX)) {
        return -1;
    } else {
        pci_num = BOND_NUM(str_len);
    }

    p = str;
    if (strncmp(p, "bond", 4) == 0) {
        p += 4;
    } else {
        return -1;
    }

    if ((*p >= '0') && (*p <= '9')) {
        mode = *p - '0';
        if (mode > BONDING_MODE_ALB) {
            return -1;
        }
        p++;
    } else {
        return -1;
    }

    if (*p != ':') {
        return -1;
    } else {
        p++;
    }

    if ((*p >= '0') && (*p <= '3')) {
        policy = *p - '0';
        p++;
    } else {
        return -1;
    }

    if (*p == '(') {
        p++;
    } else {
        return -1;
    }

    for (i = 0; i < pci_num; i++) {
        memcpy(port->pci_list[i], p, PCI_LEN);
        p += PCI_LEN;
        if (i < (pci_num - 1)) {
            if (*p != ',') {
                return -1;
            }
        } else {
            if ( *p != ')') {
                return -1;
            }
        }
        p++;
    }

    /* check dup pci */
    for (i = 0; i < pci_num; i++) {
        for (j = i + 1; j < pci_num; j++) {
            if (strcmp(port->pci_list[i], port->pci_list[j]) == 0) {
                printf("duplicate pci\n");
                return -1;
            }
        }
    }

    port->pci_num = pci_num;
    port->bond = true;
    port->bond_mode = mode;
    port->bond_policy = policy;
    return 0;
}

static int config_parse_port(int argc, char *argv[], void *data)
{
    int af_local = 0;
    int af_gateway = 0;
    struct netif_port *port = NULL;
    struct config *cfg = data;

    if ((argc < 4) || (argc > 5)) {
        return -1;
    }

    if (cfg->port_num >= NETIF_PORT_MAX) {
        return -1;
    }
    port = &cfg->ports[cfg->port_num];
    if (argv[1][0] == 'b') {
        if (config_parse_bond(port, argv[1]) < 0) {
            printf("bad bond \"%s\"\n", argv[1]);
            return -1;
        }
    } else if (strlen(argv[1]) == PCI_LEN) {
        strcpy(port->pci, argv[1]);
        port->pci_num = 1;
    } else {
        return -1;
    }

    if ((af_local = config_parse_ip(argv[2], &port->local_ip)) < 0) {
        return -1;
    }

    if ((af_gateway = config_parse_ip(argv[3], &port->gateway_ip)) < 0) {
        return -1;
    }

    if (af_local != af_gateway) {
        return -1;
    }
    port->ipv6 = af_local == AF_INET6;

    if (argc == 5) {
        if (eth_addr_init(&port->gateway_mac, argv[4]) != 0) {
            return -1;
        }
    }

    if (ipaddr_eq(&port->local_ip, &port->gateway_ip)) {
        return -1;
    }

    sprintf(port->bond_name, "net_bonding%d", cfg->port_num);
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

/*
 * return:
 *  -1       fail
 *  AF_INET  ok
 *  AF_INET6 ok
 **/
static int config_parse_ip_range(int argc, char *argv[], struct ip_range *ip_range)
{
    int af = 0;
    int num = 0;
    ipaddr_t ip;

    if (argc != 3) {
        return -1;
    }

    if ((af = config_parse_ip(argv[1], &ip)) < 0) {
        return -1;
    }

    if (ip.ip == 0) {
        return -1;
    }

    num = config_parse_number(argv[2], false, false);
    if (ip_range_init(ip_range, ip, num) < 0) {
        printf("bad client ip range %s %s\n", argv[1], argv[2]);
        return -1;
    }

    return af;
}

static int config_parse_ip_group(struct config *cfg, int argc, char *argv[], struct ip_group *ip_group)
{
    int af = 0;
    struct ip_range *ip_range = NULL;

    if (ip_group->num >= IP_RANGE_NUM_MAX) {
        return -1;
    }

    ip_range = &ip_group->ip_range[ip_group->num];
    af = config_parse_ip_range(argc, argv, ip_range);
    if (config_set_af(cfg, af) < 0) {
        return -1;
    }

    ip_group->num++;

    return 0;
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

static int config_parse_change_dip(int argc, char *argv[], void *data)
{
    int i = 0;
    int af = 0;
    int num = 0;
    int step = 0;
    ipaddr_t ip;
    struct config *cfg = data;

    if (argc != 4) {
        return -1;
    }

    if ((af = config_parse_ip(argv[1], &ip)) < 0) {
        return -1;
    }

    if ((step = atoi(argv[2])) <= 0) {
        return 0;
    }

    if ((num = atoi(argv[3])) < 0) {
        return 0;
    }

    for (i = 0; i < num; i++) {
        if (ip_list_add(&cfg->dip_list, af, &ip) < 0) {
            return -1;
        }

        ipaddr_inc(&ip, step);
    }

    return 0;
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

static int config_parse_flood(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 1) {
        return -1;
    }

    cfg->flood = true;
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

static int config_parse_wait(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = config_parse_number(argv[1], false, false);
    if (val <= 0) {
        return -1;
    }
    cfg->wait = val;

    return 0;
}

void config_set_payload(struct config *cfg, char *data, int len, int new_line)
{
    int i = 0;
    int num = 'z' - 'a' + 1;
    struct timeval tv;

    if (len == 0) {
        data[0] = 0;
        return;
    }

    if (!cfg->payload_random) {
        memset(data, 'a', len);
    } else {
        gettimeofday(&tv, NULL);
        srandom(tv.tv_usec);
        for (i = 0; i < len; i++) {
            data[i] = 'a' + random() % num;
        }
    }

    if ((len > 1) && new_line) {
        data[len - 1] = '\n';
    }

    data[len] = 0;
}

static int config_parse_payload_random(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 1) {
        return -1;
    }

    cfg->payload_random = true;
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
    if (payload_size <= 0) {
        return -1;
    }

    cfg->payload_size = payload_size;
    return 0;
}

static int config_parse_packet_size(int argc, char *argv[], void *data)
{
    int packet_size = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    packet_size = config_parse_number(argv[1], true, true);
    if (packet_size < 0) {
        return -1;
    }

    cfg->packet_size = packet_size;
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
    if (mss <= 0) {
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
#ifdef HTTP_PARSE
    } else if (strcmp(argv[1], "http") == 0) {
        cfg->protocol = IPPROTO_TCP;
        cfg->http = true;
        return 0;
#endif
    } else {
        return -1;
    }
}

static int config_parse_vxlan(int argc, char *argv[], void *data)
{
    struct config *cfg = data;
    struct vxlan *vxlan = NULL;

    if (argc != 8) {
        return -1;
    }

    if (cfg->vxlan_num >= NETIF_PORT_MAX) {
        return -1;
    }

    vxlan = &cfg->vxlans[cfg->vxlan_num];
    vxlan->vni = atoi(argv[1]);
    if ((vxlan->vni <= 0) || (vxlan->vni > VNI_MAX)) {
        printf("bad vni %s\n", argv[1]);
        return -1;
    }

    if (eth_addr_init(&vxlan->inner_smac, argv[2]) != 0) {
        printf("bad mac %s\n", argv[2]);
        return -1;
    }

    if (eth_addr_init(&vxlan->inner_dmac, argv[3]) != 0) {
        printf("bad mac %s\n", argv[3]);
        return -1;
    }

    if (config_parse_ip_range(3, &argv[3], &vxlan->vtep_local) != AF_INET) {
        return -1;
    }

    if (config_parse_ip_range(3, &argv[5], &vxlan->vtep_remote) != AF_INET) {
        return -1;
    }

    cfg->vxlan_num++;
    return 0;
}

static int config_parse_vlan(int argc, char *argv[], void *data)
{
    int vlan_id = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (cfg->vlan_id != 0) {
        printf("Error: duplicate vlan\n");
        return -1;
    }

    vlan_id = atoi(argv[1]);
    if ((vlan_id < VLAN_ID_MIN) || (vlan_id > VLAN_ID_MAX)) {
        printf("bad vlan id %s\n", argv[1]);
        return -1;
    }

    cfg->vlan_id = vlan_id;
    return 0;
}

/*
 * ret in [min, max]
 * */
static int config_parse_hex(const char *str, int min, int max, int *ret)
{
    int len = 0;
    long val = 0;
    char *end = NULL;

    len = strlen(str);
    if ((len > 2) && (str[0] == '0') && ((str[1] == 'x') || (str[1] == 'X'))) {
        val = strtol(str, &end, 16);
        if (*end != 0) {
            return -1;
        }
    } else {
        val = atoi(str);
    }

    if ((val >= min) && (val <= max)) {
        *ret = val;
        return 0;
    }

    return -1;
}

static int config_parse_tos(int argc, char *argv[], void *data)
{
    int tos = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (cfg->tos != 0) {
        printf("duplicate tos\n");
        return -1;
    }

    if (config_parse_hex(argv[1], 0, 0xff, &tos) < 0) {
        printf("invalid tos %s\n", argv[1]);
    }

    cfg->tos = tos;
    return 0;
}

static int config_parse_kni(int argc, char *argv[], void *data)
{
    struct config *cfg = data;
    const char *ifname = NULL;

    if (argc > 2) {
        return -1;
    }

    if (cfg->kni == true) {
        printf("duplicate kni\n");
        return -1;
    }

    if (argc == 2) {
        ifname = argv[1];
    } else {
        ifname = KNI_NAME_DEFAULT;
    }

    if (strlen(ifname) >= KNI_NAMESIZE) {
        printf("long kni name\n");
        return -1;
    }

    if (isalpha(ifname[0]) == 0) {
        printf("invalid kni name\n");
        return -1;
    }

    strcpy(cfg->kni_ifname, ifname);
    cfg->kni = true;

    return 0;
}

static int config_parse_jumbo(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc > 1) {
        return -1;
    }

    cfg->jumbo = true;
    return 0;
}

static int config_parse_rss(int argc, __rte_unused char *argv[], void *data)
{
    bool mq_rx_rss = true;
    struct config *cfg = data;

    if ((argc < 2) || (argc > 3)) {
        return -1;
    }

    if (cfg->rss != RSS_NONE) {
        printf("Error: duplicate rss\n");
        return -1;
    }

    if (argc >= 2) {
        if (strcmp(argv[1], "l3") == 0) {
            cfg->rss = RSS_L3;
        } else if (strcmp(argv[1], "l3l4") == 0) {
            cfg->rss = RSS_L3L4;
        } else if (strcmp(argv[1], "auto") == 0) {
            cfg->rss = RSS_AUTO;
        } else {
            printf("Error: unknown rss type \'%s\'\n", argv[1]);
            return -1;
        }

        if (argc == 3) {
            if (strcmp(argv[2], "mq_rx_rss") == 0) {
                mq_rx_rss = true;
            } else if (strcmp(argv[2], "mq_rx_none") == 0) {
                if (cfg->rss != RSS_AUTO) {
                    printf("Error: rss type \'%s\' dose not support \'mq_rx_none\'\n", argv[1]);
                    return -1;
                }
                mq_rx_rss = false;
            } else if ((cfg->rss == RSS_AUTO) && ((strcmp(argv[2], "l3") == 0))) {
                cfg->rss_auto = RSS_L3;
                mq_rx_rss = true;
            } else if ((cfg->rss == RSS_AUTO) && ((strcmp(argv[2], "l3l4") == 0))) {
                cfg->rss_auto = RSS_L3L4;
                mq_rx_rss = true;
            } else {
                printf("Error: unknown rss config \'%s\'\n", argv[2]);
                return -1;
            }
        }
    } else {
        cfg->rss = RSS_L3;
    }

    cfg->mq_rx_rss = mq_rx_rss;

    return 0;
}

static int config_parse_quiet(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc > 1) {
        return -1;
    }

    if (cfg->quiet == true) {
        printf("Error: duplicate quiet\n");
        return -1;
    }
    cfg->quiet = true;
    return 0;
}

static int config_parse_tcp_rst(int argc, char *argv[], void *data)
{
    int val = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    val = atoi(argv[1]);
    if ((val == 0) || (val == 1)) {
        cfg->tcp_rst = val;
    } else {
        return -1;
    }

    return 0;
}

static int config_parse_http_host(int argc, char *argv[], void *data)
{
    int len = 0;
    struct config *cfg = data;

    if (argc != 2) {
        return -1;
    }

    if (cfg->http_host[0] != 0) {
        return -1;
    }

    len = strlen(argv[1]);
    if (len >= HTTP_HOST_MAX) {
        return -1;
    }

    strcpy(cfg->http_host, argv[1]);
    return 0;
}

static int config_parse_http_path(int argc, char *argv[], void *data)
{
    int len = 0;
    struct config *cfg = data;
    const char *path = NULL;

    if (argc != 2) {
        return -1;
    }

    if (cfg->http_path[0] != 0) {
        return -1;
    }

    path = argv[1];
    len = strlen(path);
    if (len >= HTTP_PATH_MAX) {
        return -1;
    }

    if (path[0] != '/') {
        return -1;
    }

    strcpy(cfg->http_path, path);
    return 0;
}

static int config_parse_lport_range(int argc, char *argv[], void *data)
{
    int lport_min = 1;
    int lport_max = NETWORK_PORT_NUM - 1;
    struct config *cfg = data;

    if ((argc < 2) || (argc > 3)) {
        return -1;
    }

    if ((cfg->lport_min != 0) || (cfg->lport_max != 0)) {
        return -1;
    }

    lport_min = atoi(argv[1]);
    if ((lport_min <= 0) || (lport_min >= NETWORK_PORT_NUM)) {
        return -1;
    }

    if (argc == 3) {
        lport_max = atoi(argv[2]);
        if ((lport_max <= 0) || (lport_max >= NETWORK_PORT_NUM)) {
            return -1;
        }
    }

    if (lport_min >= lport_max) {
        return -1;
    }

    cfg->lport_min = lport_min;
    cfg->lport_max = lport_max;

    return 0;
}

static int config_parse_client_hop(int argc, __rte_unused char *argv[], void *data)
{
    struct config *cfg = data;

    if (argc != 1) {
        return -1;
    }

    cfg->client_hop = true;
    return 0;
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
            if (cfg->vxlan) {
                printf("Error: 'vxlan' requires cpu num to be equal to server ip num\n");
                return -1;
            }

            if (port->queue_num < port->server_ip_range.num) {
                printf("Error: cpu num less than server ip num at 'client' mode\n");
                return -1;
            }

            if (cfg->flood) {
                if (cfg->dip_list.num) {
                    continue;
                }

                if (cfg->rss != RSS_L3L4) {
                    cfg->rss = RSS_L3L4;
                    printf("Warning: 'rss l3l4' is enabled\n");
                }
            } else if (cfg->rss == RSS_NONE) {
                printf("Error: 'rss' is required if cpu num is not equal to server ip num\n");
                return -1;
            }
        }
        i++;
    }

    return 0;
}

static int config_check_vlan(struct config *cfg)
{
    struct netif_port *port = NULL;

    if (cfg->vlan_id == 0) {
        return 0;
    }

    if (cfg->vxlan_num) {
        printf("Error: Cannot enable vlan and vxlan at the same time\n");
        return -1;
    }

    config_for_each_port(cfg, port) {
        if (port->bond) {
            printf("Error: Cannot enable vlan and bond at the same time\n");
            return -1;
        }
    }

    return 0;
}

static int config_check_vxlan(struct config *cfg)
{
    int i = 0;
    struct vxlan *vxlan = NULL;
    struct netif_port *port = NULL;

    if (cfg->vxlan_num == 0) {
        return 0;
    }

    if (cfg->vxlan_num != cfg->port_num) {
        printf("The number of 'vxlan' and 'port' are not equal.\n");
        return -1;
    }

    config_for_each_port(cfg, port) {
        vxlan = &cfg->vxlans[i];
        if (vxlan->vtep_local.num != port->queue_num) {
            printf("The number of vtep_local and queue_num are not equal.\n");
            return -1;
        }

        if ((vxlan->vtep_remote.num > 1) && (vxlan->vtep_remote.num != port->queue_num)) {
            printf("Bad vtep_remote num.\n");
            return -1;
        }

        port->vxlan = vxlan;
        i++;
    }

    cfg->vxlan = true;
    return 0;
}

static int config_check_af(struct config *cfg)
{
    struct netif_port *port = NULL;

    config_for_each_port(cfg, port) {
        if (cfg->vxlan) {
            if (port->ipv6) {
                printf("Underlay address not support IPV6.\n");
                return -1;
            }
        } else {
            if (cfg->ipv6 != port->ipv6) {
                printf("Bad port address.\n");
                return -1;
            }
        }
    }

    return 0;
}

static int config_check_port_pci(struct netif_port *port0, struct netif_port *port1)
{
    int i = 0;
    int j = 0;

    for (i = 0; i < port0->pci_num; i++) {
        for (j = 0; j < port1->pci_num; j++) {
            if (strcmp(port0->pci_list[i], port1->pci_list[j]) == 0) {
                return -1;
            }
        }
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

            if (config_check_port_pci(port0, port1) < 0) {
                printf("duplicate pci\n");
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

static int config_check_address_confliec_port(const struct config *cfg, const struct ip_group *ipg, int local)
{
    const ipaddr_t *addr = NULL;
    const struct netif_port *port = NULL;
    const struct ip_range *ip_range = NULL;

    config_for_each_port(cfg, port) {
        if (local) {
            addr = &port->local_ip;
        } else {
            addr = &port->gateway_ip;
        }
        for_each_ip_range(ipg, ip_range) {
            if (port->ipv6) {
                if (ip_range_exist_ipv6(ip_range, addr)) {
                    return -1;
                }
            } else {
                if (ip_range_exist(ip_range, addr->ip)) {
                    return -1;
                }
            }
        }
    }

    return 0;
}

static int config_check_address_conflict(const struct config *cfg)
{
    const struct ip_group *cipg = NULL;
    const struct ip_group *sipg = NULL;
    const struct ip_range *ip_range0 = NULL;
    const struct ip_range *ip_range1 = NULL;

    cipg = &cfg->client_ip_group;
    sipg = &cfg->server_ip_group;
    for_each_ip_range(cipg, ip_range0) {
        for_each_ip_range(sipg, ip_range1) {
            if (config_ip_range_overlap(ip_range0, ip_range1)) {
                printf("Error: client and server address conflict\n");
                return -1;
            }
        }
    }

    /*
     * client mode:
     *  local ip cannot in server ip rage
     *  gateway ip cannot in client ip rage
     * server mode:
     *  local ip cannot in client ip range
     *  gateway ip cannot in server ip range
     * */
    if (cfg->server) {
        if (config_check_address_confliec_port(cfg, cipg, 1) < 0) {
            printf("Error: local ip conflict with client address\n");
            return -1;
        }

        if (config_check_address_confliec_port(cfg, sipg, 0) < 0) {
            printf("Error: gateway ip conflict with server address\n");
            return -1;
        }
    } else {
        if (config_check_address_confliec_port(cfg, sipg, 1) < 0) {
            printf("Error: local ip conflict with server address\n");
            return -1;
        }

        if (config_check_address_confliec_port(cfg, cipg, 0) < 0) {
            printf("Error: gateway ip conflict with client address\n");
            return -1;
        }
    }

    return 0;
}

static int config_check_wait(struct config *cfg)
{
    if (cfg->server) {
        if (cfg->wait != 0) {
            printf("Error: wait in server config\n");
            return -1;
        }
        return 0;
    }

    if (cfg->wait == 0) {
        cfg->wait = WAIT_DEFAULT;
    }

    return 0;
}

static int config_check_slow_start(struct config *cfg)
{
    if (cfg->server) {
        if (cfg->slow_start != 0) {
            printf("Error: slow_start in server config\n");
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

    if (cfg->server) {
        cfg->cc = 0;
        cfg->cps = 0;
        cfg->flood = false;
    } else {
        if ((cfg->cps == 0) && (cfg->cc == 0)) {
            printf("Error: no targets\n");
            return -1;
        }

        if (cfg->cps == 0) {
            cfg->cps = DEFAULT_CPS;
        }
    }

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

static int config_packet_headers_size(struct config *cfg)
{
    int size = 0;

    if (cfg->vxlan) {
        size += VXLAN_HEADERS_SIZE;
    }

    size += sizeof(struct eth_hdr);

    if (cfg->ipv6) {
        size += sizeof(struct ip6_hdr);
    } else {
        size += sizeof(struct iphdr);
    }

    if (cfg->protocol == IPPROTO_TCP) {
        size += sizeof(struct tcphdr);
    } else {
        size += sizeof(struct udphdr);
    }

    return size;
}

static int config_check_size(struct config *cfg)
{
    int payload_size = 0;
    int packet_size_max = PACKET_SIZE_MAX;
    int headers_size = 0;


    if ((cfg->packet_size != 0) && (cfg->payload_size != 0)) {
        printf("Error: both payload_size and packet_size are set\n");
        return -1;
    }

    if (cfg->jumbo) {
        packet_size_max = JUMBO_PKT_SIZE_MAX;
    }

    headers_size = config_packet_headers_size(cfg);

    if (cfg->packet_size) {
        if (cfg->packet_size <= headers_size) {
            printf("Error: small packet_size %d\n", cfg->packet_size);
            return -1;
        }

        if (cfg->packet_size > packet_size_max) {
            printf("Error: big packet_size %d\n", cfg->packet_size);
            return -1;
        }
        payload_size = cfg->packet_size - headers_size;
    } else if (cfg->payload_size) {
        if ((cfg->payload_size + headers_size) > packet_size_max) {
            printf("Error: big payload_size %d\n", cfg->payload_size);
            return -1;
        }

        if ((cfg->protocol == IPPROTO_TCP) && (cfg->payload_size < HTTP_DATA_MIN_SIZE)) {
            payload_size = HTTP_DATA_MIN_SIZE;
        } else {
            payload_size = cfg->payload_size;
        }
    }

    if ((cfg->protocol == IPPROTO_TCP) && ((payload_size == 0) || (payload_size >= HTTP_DATA_MIN_SIZE))) {
        cfg->stats_http = true;
    }

    http_set_payload(cfg, payload_size);
    udp_set_payload(cfg, payload_size);

    return 0;
}

static int config_check_mss(struct config *cfg)
{
    int mss_max = 0;

    if (cfg->ipv6) {
        if (cfg->jumbo) {
            mss_max = MSS_JUMBO_IPV6;
        } else {
            mss_max = MSS_IPV6;
        }
    } else {
        if (cfg->jumbo) {
            mss_max = MSS_JUMBO_IPV4;
        } else {
            mss_max = MSS_IPV4;
        }
    }

    if (cfg->mss > mss_max) {
        printf("Error: bad mss %d\n", cfg->mss);
        return -1;
    }

    if (cfg->mss == 0) {
        cfg->mss = mss_max;
    }

    return 0;
}

static int config_server_addr_check_num(struct config *cfg, int num)
{
    const struct ip_range *ipr = NULL;

    for_each_ip_range(&(cfg->server_ip_group), ipr) {
        if (ipr->num != num) {
            return -1;
        }
    }

    return 0;
}

static int config_check_rss(struct config *cfg)
{
    if (cfg->rss == RSS_NONE) {
        return 0;
    }

    if (cfg->cpu_num == cfg->port_num) {
        printf("Warnning: rss is disabled\n");
        cfg->rss = RSS_NONE;
    }

    if (cfg->vxlan) {
        printf("Error: rss is not supported for vxlan.\n");
        return -1;
    }

    if (cfg->flood) {
        if ((cfg->rss == RSS_AUTO) || (cfg->rss == RSS_L3)) {
            printf("Error: \'rss auto|l3\' conflicts with \'flood\'\n");
            return -1;
        }
    }

    /* must be 1 server ip */
    if (cfg->rss == RSS_AUTO) {
        if (config_server_addr_check_num(cfg, 1) != 0) {
            printf("Error: rss \'auto\' requires one server address.\n");
            return -1;
        }
    }

    return 0;
}

static int config_check_keepalive(struct config *cfg)
{
    int ticks_per_sec = 0;
    if (cfg->server == 0) {
        if (cfg->cc && (cfg->keepalive == false)) {
            printf("Error: 'cc' requires 'keepalive'\n");
            return -1;
        }

        if (cfg->keepalive == false) {
            return 0;
        }

        if (cfg->flood && (cfg->keepalive_request_interval_us == 0)) {
            printf("Error: 'flood' requires a positive keepalive request interval\n");
            return -1;
        }

        /* interval is less 100us, eg 10us, 20us ...  */
        if (cfg->keepalive_request_interval_us == 1) {
            /* 0.5 us */
            ticks_per_sec = 1000 * 1000 * 2;
        } else if (cfg->keepalive_request_interval_us == 2) {
            /* 1 us */
            ticks_per_sec = 1000 * 1000;
        } else if (cfg->keepalive_request_interval_us < 10) {
            /* 2 us */
            ticks_per_sec = 1000 * 500;
        } else if (cfg->keepalive_request_interval_us < 50) {
           /* 5 us */
            ticks_per_sec = 1000 * 100 * 2;
        } else if (cfg->keepalive_request_interval_us < 100) {
            /* 10 us */
            ticks_per_sec = 1000 * 100;
        } else if (cfg->keepalive_request_interval_us < 500) {
            /* 50 us */
            ticks_per_sec = 1000 * 10 * 2;
        } else if (cfg->keepalive_request_interval_us < 1000) {
            /* 100 us */
            ticks_per_sec = 1000 * 10;
        } else {
            ticks_per_sec = TICKS_PER_SEC_DEFAULT;
        }
        cfg->ticks_per_sec = ticks_per_sec;
    } else {
        cfg->keepalive_request_num = 0;
    }

    return 0;
}

static int config_check_pipeline(struct config *cfg)
{
    if (cfg->pipeline == 0) {
        return 0;
    }

    if (cfg->server) {
        printf("Error: \'pipeline\' cannot set in server mode\n");
        return -1;
    }

    if (cfg->protocol == IPPROTO_TCP) {
        printf("Error: \'pipeline\' cannot support tcp\n");
        return -1;
    }

    if (!cfg->keepalive) {
        printf("Error: \'pipeline\' requires \'keepalive\'\n");
        return -1;
    }

    if ((cfg->flood == false) && (cfg->keepalive_request_interval_us)) {
        printf("Error: \'pipeline\' requires zero keepalive interval\n");
        return -1;
    }

    return 0;
}

static int config_check_change_dip(struct config *cfg)
{
    struct ip_list *ip_list = NULL;

    ip_list = &cfg->dip_list;
    if (ip_list->num == 0) {
        return 0;
    }
    if (cfg->server) {
        printf("Error: \'change_dip\' only support client mode\n");
        return -1;
    }

    if (!cfg->flood) {
        printf("Error: \'change_dip\' only support flood mode\n");
        return -1;
    }

    if (cfg->vxlan) {
        printf("Error: \'change_dip\' not support vxlan\n");
        return -1;
    }

    if ((ip_list->af == AF_INET6) != cfg->ipv6) {
        printf("Error: bad ip address family of \'change_dip\'\n");
        return -1;
    }

    if (ip_list->num < cfg->cpu_num) {
        printf("Error: number of \'change_dip\' is less than cpu number\n");
        return -1;
    }

    return 0;
}

static int config_check_http(struct config *cfg)
{
    int http_host = 0;
    int http_path = 0;

    http_host = (cfg->http_host[0] != 0);
    http_path = (cfg->http_path[0] != 0);

    if (cfg->packet_size || cfg->payload_size) {
        if (http_host || http_path) {
            printf("Error: The HTTP host/path cannot be set with packet_size or payload_size.\n");
            return -1;
        }
    }

    if (cfg->server) {
        if (http_host || http_path) {
            printf("Error: the HTTP host/path cannot be set in server mode.\n");
            return -1;
        }
    }

    if (cfg->protocol == IPPROTO_UDP) {
        if (http_host || http_path) {
            printf("Error: The HTTP host/path cannot be set in udp protocol.\n");
            return -1;
        }
    }

    if (!http_host) {
        strcpy(cfg->http_host, HTTP_HOST_DEFAULT);
    }

    if (!http_path) {
        strcpy(cfg->http_path, HTTP_PATH_DEFAULT);
    }

    if (cfg->http) {
        cfg->stats_http = true;
    }

    return 0;
}

static void config_check_lport_range(struct config *cfg)
{
    if (cfg->lport_min == 0) {
        cfg->lport_min = 1;
    }

    if (cfg->lport_max == 0) {
        cfg->lport_max = NETWORK_PORT_NUM - 1;
    }
}

int config_parse(int argc, char **argv, struct config *cfg)
{
    int conf = 0;
    int opt = 0;
    int test = 0;
    const char *optstr = "hvtmc:";

    if (argc == 1) {
        config_help();
        return -1;
    }

    while ((opt = getopt_long_only(argc, argv, optstr, g_options, NULL)) != -1) {
        switch (opt) {
            case 'c':
                if (config_keyword_parse(optarg, g_config_keywords, cfg) < 0) {
                    return -1;
                }
                conf = 1;
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

    if (conf == 0) {
        printf("No configuration file\n");
        return -1;
    }

    if (cfg->protocol == 0) {
        cfg->protocol = IPPROTO_TCP;
    }

    if (cfg->duration == 0) {
        cfg->duration = DEFAULT_DURATION;
    }

    if (config_check_mss(cfg) < 0) {
        return -1;
    }

    if ((cfg->listen == 0) || (cfg->listen_num == 0)) {
        cfg->listen = 80;
        cfg->listen_num = 1;
    }

    if (config_check_keepalive(cfg) < 0) {
        return -1;
    }

    if (config_check_pipeline(cfg) < 0) {
        return -1;
    }

    /* called before config_check_size() */
    if (config_check_http(cfg) < 0) {
        return -1;
    }

    if (cfg->launch_num == 0) {
        cfg->launch_num = DEFAULT_LAUNCH;
    }

    if (config_check_wait(cfg) < 0) {
        return -1;
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

    if (config_check_address_conflict(cfg) < 0) {
        return -1;
    }

    if (config_check_local_addr(cfg) < 0) {
        return -1;
    }

    if (config_check_size(cfg) < 0) {
        return -1;
    }

    if (config_check_port(cfg) != 0) {
        return -1;
    }

    if (config_check_vxlan(cfg) < 0) {
        return -1;
    }

    if (config_check_vlan(cfg) < 0) {
        return -1;
    }

    if (config_check_af(cfg) < 0) {
        return -1;
    }

    if (config_check_rss(cfg) < 0) {
        return -1;
    }

    if (config_set_port_ip_range(cfg) != 0) {
        return -1;
    }

    if (config_check_change_dip(cfg) < 0) {
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

    config_check_lport_range(cfg);

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

void config_set_tsc(struct config *cfg, uint64_t hz)
{
    uint64_t us = 0;
    uint64_t ms = 0;
    uint64_t tsc = 0;

    us = cfg->keepalive_request_interval_us;

    /* ms */
    if ((us % 1000) == 0) {
        ms = us / 1000;
        tsc = ms * (hz / 1000);
    } else {
        tsc = (us * (hz / 1000)) / 1000;
    }

    cfg->keepalive_request_interval = tsc;
}

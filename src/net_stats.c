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
 * Author: Jianzhang Peng (pengjianzhang@gmail.com)
 *         Jianzhang Peng (pengjianzhang@gmail.com)
 */

#include "net_stats.h"

#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "cpuload.h"
#include "work_space.h"
#include "version.h"

static struct net_stats *g_net_stats_all[THREAD_NUM_MAX];
static struct net_stats g_net_stats_total;
__thread struct net_stats g_net_stats;

#define NET_STATS_CLOUR_ON          "\033[41;37m"
#define NET_STATS_CLOUR_OFF         "\033[0m"
#define NET_STATS_ERR_FMT           NET_STATS_CLOUR_ON"%s"NET_STATS_CLOUR_OFF

#define STATS_BUF_LEN               64
#define NET_STATS(s, i)             (((uint64_t*)(s))[(i)])
#define NET_STATS_ELEMENTS_NUM      (int)(sizeof(struct net_stats) / sizeof(uint64_t))
#define NET_STATS_INC_ELEMENTS_NUM  (int)((offsetof(struct net_stats, mutable_start)) / sizeof(uint64_t))

#define FOR_EACH_NET_STATS(s, i)   for (i = 0; (i < g_config.cpu_num) && ((s = g_net_stats_all[i]), 1); i++)

static void net_stats_format_print2(uint64_t val, char *buf, int len)
{
    int i = 0;
    uint64_t values[5] = {0, 0, 0, 0, 0};

    for (i = 0; i < 5; i++) {
        values[i] = val % 1000;
        val = val / 1000;
    }

    if (values[4]) {
        snprintf(buf, len, "%lu,%03lu,%03lu,%03lu,%03lu", values[4], values[3], values[2], values[1], values[0]);
    } else if (values[3]) {
        snprintf(buf, len, "%lu,%03lu,%03lu,%03lu", values[3], values[2], values[1], values[0]);
    } else if (values[2]) {
        snprintf(buf, len, "%lu,%03lu,%03lu", values[2], values[1], values[0]);
    } else if (values[1]) {
        snprintf(buf, len, "%lu,%03lu", values[1], values[0]);
    } else {
        snprintf(buf, len, "%lu", values[0]);
    }
}

static void net_stats_format_print3(uint64_t val, char *buf, int len, int space, int err)
{
    int i = 0;
    int n = 0;
    int str_len = 0;
    char str[STATS_BUF_LEN] = {0};
    char *p = NULL;
    char *last = NULL;

    net_stats_format_print2(val, str, STATS_BUF_LEN);
    str_len = strlen(str);

    if ((err == 0) || (val == 0)) {
        snprintf(buf, len, "%s", str);
    } else {
        snprintf(buf, len, NET_STATS_ERR_FMT, str);
    }

    n = space - str_len;
    if (n <= 0) {
        return;
    }

    p = buf + strlen(buf);
    last = buf + len - 1;
    for (i = 0; i < n; i++) {
        if (p <= last) {
            *p = ' ';
            p++;
        } else {
            break;
        }
    }
    if (p <= last) {
        *p = 0;
    } else {
        *last = 0;
    }
}

#define net_stats_format_print(val, buf, len)           net_stats_format_print3(val, buf, len, 18, 0)
#define net_stats_format_print_err(val, buf, len)       net_stats_format_print3(val, buf, len, 18, 1)
#define net_stats_format_print_short_err(val, buf, len) net_stats_format_print3(val, buf, len, 10, 1)

#define SNPRINTF(p, len, fmt...) do {           \
    int ret = snprintf(p, len, fmt);            \
    if (ret >= len) {                           \
        goto err;                               \
    } else {                                    \
        p += ret;                               \
        len -= ret;                             \
    }                                           \
} while (0)

static void net_stats_print_rtt(struct net_stats *stats, char rtt_str[], int len)
{
    uint64_t rtt_tsc = stats->rtt_tsc;
    uint64_t rtt_num = stats->rtt_num;
    uint64_t tsc_per_us = TSC_PER_SEC / (1000 * 1000);
    uint64_t rtt_us = 0;
    uint64_t rtt_us_minor = 0;
    char rtt[STATS_BUF_LEN];
    char rtt2[STATS_BUF_LEN];

    if (rtt_num > 0) {
        rtt_us = rtt_tsc / (rtt_num * tsc_per_us);
        rtt_us_minor = ((rtt_tsc % (rtt_num * tsc_per_us)) * 10) / (rtt_num * tsc_per_us);
    }

    net_stats_format_print2(rtt_us, rtt, STATS_BUF_LEN);
    snprintf(rtt2, STATS_BUF_LEN, "%s.%lu", rtt, rtt_us_minor);
    snprintf(rtt_str, len, "%-10s", rtt2);
}

static int net_stats_print_socket(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    char open[STATS_BUF_LEN];
    char close[STATS_BUF_LEN];
    char error[STATS_BUF_LEN];
    char curr[STATS_BUF_LEN];
    char rtt[STATS_BUF_LEN];
    int len = buf_len;
    uint64_t sk_open = 0;
    uint64_t sk_close = 0;

    sk_open = stats->socket_open - stats->socket_dup;
    sk_close = stats->socket_close;
    if (sk_close >= stats->socket_dup) {
        sk_close -= stats->socket_dup;
    }

    net_stats_format_print(sk_open, open, STATS_BUF_LEN);
    net_stats_format_print(sk_close, close, STATS_BUF_LEN);
    net_stats_format_print_err(stats->socket_error, error, STATS_BUF_LEN);
    net_stats_format_print(stats->socket_current, curr, STATS_BUF_LEN);

    if ((g_config.server) || (g_config.keepalive)) {
        SNPRINTF(p, len, "skOpen  %s skClose  %s skCon    %s skErr   %s\n", open, close, curr, error);
    } else {
        net_stats_print_rtt(stats, rtt, STATS_BUF_LEN);
        SNPRINTF(p, len, "skOpen  %s skClose  %s skCon    %s skErr   %s rtt(us) %s\n", open, close, curr, error, rtt);
    }
    return p - buf;

err:
    return -1;
}

static int net_stats_print_tcp(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    int len = buf_len;
    char tcp_req[STATS_BUF_LEN];
    char tcp_rsp[STATS_BUF_LEN];

    net_stats_format_print(stats->tcp_req, tcp_req, STATS_BUF_LEN);
    net_stats_format_print(stats->tcp_rsp, tcp_rsp, STATS_BUF_LEN);

    SNPRINTF(p, len, "tcpReq  %s tcpRsp   %s\n", tcp_req, tcp_rsp);
    return p - buf;

err:
    return -1;
}

static int net_stats_print_http(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    int len = buf_len;

    char http_error[STATS_BUF_LEN];
    char http_2xx[STATS_BUF_LEN];
    char http_get[STATS_BUF_LEN];
    char http_post[STATS_BUF_LEN];

    net_stats_format_print(stats->http_2xx, http_2xx, STATS_BUF_LEN);
    net_stats_format_print(stats->http_get, http_get, STATS_BUF_LEN);
    net_stats_format_print(stats->http_post, http_post, STATS_BUF_LEN);
    net_stats_format_print_err(stats->http_error, http_error, STATS_BUF_LEN);

    SNPRINTF(p, len, "httpGet %s httpPost %s http2XX  %s httpErr %s\n", http_get, http_post, http_2xx, http_error);
    return p - buf;

err:
    return -1;
}

static int net_stats_print_pkt(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    int len = buf_len;

    char pkt_rx[STATS_BUF_LEN];
    char pkt_tx[STATS_BUF_LEN];
    char bits_rx[STATS_BUF_LEN];
    char bits_tx[STATS_BUF_LEN];
    char tx_drop[STATS_BUF_LEN];

    net_stats_format_print(stats->pkt_rx, pkt_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->pkt_tx, pkt_tx, STATS_BUF_LEN);
    net_stats_format_print(((uint64_t)stats->byte_rx) * 8, bits_rx, STATS_BUF_LEN);
    net_stats_format_print(((uint64_t)stats->byte_tx) * 8, bits_tx, STATS_BUF_LEN);
    net_stats_format_print_err(stats->tx_drop, tx_drop, STATS_BUF_LEN);

    SNPRINTF(p, len, "pktRx   %s pktTx    %s bitsRx   %s bitsTx  %s dropTx  %s\n",
                    pkt_rx, pkt_tx, bits_rx, bits_tx, tx_drop);
    return p - buf;

err:
    return -1;
}

static int net_stats_print_tcp_flags(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    char rst_rx[STATS_BUF_LEN];
    char rst_tx[STATS_BUF_LEN];
    char fin_rx[STATS_BUF_LEN];
    char fin_tx[STATS_BUF_LEN];
    char syn_rx[STATS_BUF_LEN];
    char syn_tx[STATS_BUF_LEN];
    int len = buf_len;

    net_stats_format_print(stats->syn_rx, syn_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->syn_tx, syn_tx, STATS_BUF_LEN);

    net_stats_format_print(stats->fin_rx, fin_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->fin_tx, fin_tx, STATS_BUF_LEN);

    net_stats_format_print_short_err(stats->rst_rx, rst_rx, STATS_BUF_LEN);
    net_stats_format_print_short_err(stats->rst_tx, rst_tx, STATS_BUF_LEN);

    SNPRINTF(p, len, "synRx   %s synTx    %s finRx    %s finTx   %s rstRx   %s rstTx %s\n",
                    syn_rx, syn_tx, fin_rx, fin_tx, rst_rx, rst_tx);
    return p - buf;

err:
    return -1;
}

static int net_stats_print_proto_rx_tx(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    char arp_rx[STATS_BUF_LEN];
    char arp_tx[STATS_BUF_LEN];
    char icmp_rx[STATS_BUF_LEN];
    char icmp_tx[STATS_BUF_LEN];
    char tcp_rx[STATS_BUF_LEN];
    char tcp_tx[STATS_BUF_LEN];
    char udp_rx[STATS_BUF_LEN];
    char udp_tx[STATS_BUF_LEN];
    char tos_rx[STATS_BUF_LEN];
    char other_rx[STATS_BUF_LEN];
    char rx_bad[STATS_BUF_LEN];
    int len = buf_len;

    net_stats_format_print(stats->arp_rx, arp_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->arp_tx, arp_tx, STATS_BUF_LEN);
    net_stats_format_print(stats->icmp_rx, icmp_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->icmp_tx, icmp_tx, STATS_BUF_LEN);
    net_stats_format_print(stats->udp_rx, udp_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->udp_tx, udp_tx, STATS_BUF_LEN);
    net_stats_format_print(stats->tcp_rx, tcp_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->tcp_tx, tcp_tx, STATS_BUF_LEN);
    net_stats_format_print(stats->tos_rx, tos_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->other_rx, other_rx, STATS_BUF_LEN);
    net_stats_format_print_err(stats->rx_bad, rx_bad, STATS_BUF_LEN);

    SNPRINTF(p, len, "tcpRx   %s tcpTx    %s udpRx    %s udpTx   %s\n",
                    tcp_rx, tcp_tx, udp_rx, udp_tx);

    SNPRINTF(p, len, "arpRx   %s arpTx    %s icmpRx   %s icmpTx  %s\n",
                    arp_rx, arp_tx, icmp_rx, icmp_tx);

    SNPRINTF(p, len, "tosRx   %s otherRx  %s badRx    %s\n",
                    tos_rx, other_rx, rx_bad);

    return p - buf;

err:
    return -1;
}

static int net_stats_print_retransmit(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    char syn_rt[STATS_BUF_LEN];
    char fin_rt[STATS_BUF_LEN];
    char ack_rt[STATS_BUF_LEN];
    char push_rt[STATS_BUF_LEN];
    char tcp_drop[STATS_BUF_LEN];
    char ack_dup[STATS_BUF_LEN];

    char udp_rt[STATS_BUF_LEN];
    char udp_drop[STATS_BUF_LEN];
    int len = buf_len;

    net_stats_format_print_err(stats->tcp_drop, tcp_drop, STATS_BUF_LEN);
    net_stats_format_print_err(stats->udp_drop, udp_drop, STATS_BUF_LEN);
    if (g_config.protocol == IPPROTO_TCP) {
        net_stats_format_print_err(stats->syn_rt, syn_rt, STATS_BUF_LEN);
        net_stats_format_print_err(stats->fin_rt, fin_rt, STATS_BUF_LEN);
        net_stats_format_print_err(stats->ack_rt, ack_rt, STATS_BUF_LEN);
        net_stats_format_print_err(stats->push_rt, push_rt, STATS_BUF_LEN);
        net_stats_format_print_err(stats->ack_dup, ack_dup, STATS_BUF_LEN);

        SNPRINTF(p, len, "synRt   %s finRt    %s ackRt    %s pushRt  %s\n",
            syn_rt, fin_rt, ack_rt, push_rt);
        SNPRINTF(p, len, "tcpDrop %s udpDrop  %s ackDup   %s\n", tcp_drop, udp_drop, ack_dup);
    } else {
        net_stats_format_print_err(stats->udp_rt, udp_rt, STATS_BUF_LEN);
        SNPRINTF(p, len, "udpRt   %s udpDrop  %s tcpDrop  %s\n", udp_rt, udp_drop, tcp_drop);
    }
    return p - buf;

err:
    return -1;
}

static int net_stats_print_kni(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    char kni_rx[STATS_BUF_LEN];
    char kni_tx[STATS_BUF_LEN];
    int len = buf_len;

    net_stats_format_print(stats->kni_rx, kni_rx, STATS_BUF_LEN);
    net_stats_format_print(stats->kni_tx, kni_tx, STATS_BUF_LEN);

    SNPRINTF(p, len, "kniRx   %s kniTx    %s\n", kni_rx, kni_tx);

    return p - buf;
err:
    return -1;
}

#define buf_skip(p, len, skip) do { \
    if (skip < 0) {                 \
        goto err;                   \
    }                               \
    p += skip;                      \
    len -= skip;                    \
} while (0)

static int net_stats_print(struct net_stats *stats, char *buf, int buf_len)
{
    char *p = buf;
    int len = buf_len;
    int ret = 0;

    ret = net_stats_print_pkt(stats, p, len);
    buf_skip(p, len, ret);

    if (g_config.kni) {
        ret = net_stats_print_kni(stats, p, len);
        buf_skip(p, len, ret);
    }

    ret = net_stats_print_proto_rx_tx(stats, p, len);
    buf_skip(p, len, ret);

    if (g_config.protocol == IPPROTO_TCP) {
        ret = net_stats_print_tcp_flags(stats, p, len);
        buf_skip(p, len, ret);
    }

    ret = net_stats_print_retransmit(stats, p, len);
    buf_skip(p, len, ret);

    ret = net_stats_print_socket(stats, p, len);
    buf_skip(p, len, ret);

    if (g_config.protocol == IPPROTO_TCP) {
        if (g_config.stats_http) {
            ret = net_stats_print_http(stats, p, len);
        } else {
            ret = net_stats_print_tcp(stats, p, len);
        }
        buf_skip(p, len, ret);
    }

    return p - buf;

err:
    return -1;
}

static void net_stats_assign(struct net_stats *result, struct net_stats *s)
{
    memcpy(result, s, sizeof(struct net_stats));
}

static void net_stats_add(struct net_stats *result, struct net_stats *s)
{
    int i = 0;

    for (i = 0; i < NET_STATS_ELEMENTS_NUM; i++) {
        NET_STATS(result, i) += NET_STATS(s, i);
    }
}

/* called every 1 second */
static void net_stats_del(struct net_stats *result, struct net_stats *s0, struct net_stats *s1)
{
    int i = 0;

    for (i = 0; i < NET_STATS_ELEMENTS_NUM; i++) {
        if (NET_STATS(s0, i) > NET_STATS(s1, i)) {
            NET_STATS(result, i) = NET_STATS(s0, i) - NET_STATS(s1, i);
        } else {
            NET_STATS(result, i) = 0;
        }
    }
}

static void net_stats_clear(struct net_stats *s)
{
    memset(s, 0, sizeof(struct net_stats));
}

static void net_stats_clear_mutable(struct net_stats *s)
{
    /* don't clear rtt */
    s->cpusage = 0;
    s->socket_current = 0;
}

static void net_stats_sum(struct net_stats *result)
{
    int i = 0;
    struct net_stats *s = NULL;

    net_stats_clear(result);
    FOR_EACH_NET_STATS(s, i) {
        net_stats_add(result, s);
    }
}

static void net_stats_get_speed(struct net_stats *speed)
{
    struct net_stats sum;

    net_stats_sum(&sum);
    net_stats_del(speed, &sum, &g_net_stats_total);
    net_stats_assign(&g_net_stats_total, &sum);
    net_stats_clear_mutable(&g_net_stats_total);
}

static int net_stats_cpusage_print(char *buf, int buf_len)
{
    int i = 0;
    char *p = buf;
    int len = buf_len;
    struct net_stats *s = NULL;

    SNPRINTF(p, len, " cpuUsage ");
    FOR_EACH_NET_STATS(s, i) {
        SNPRINTF(p, len, "%-4lu", s->cpusage);
    }

    SNPRINTF(p, len, "\n");
    return p - buf;

err:
    return -1;
}

static void net_stats_print_eth(FILE *fp)
{
    struct rte_eth_stats st;
    struct netif_port *port = NULL;
    char ierrors[STATS_BUF_LEN];
    char oerrors[STATS_BUF_LEN];
    char imissed[STATS_BUF_LEN];
    uint64_t ierr = 0;
    uint64_t oerr = 0;
    uint64_t imis = 0;

    config_for_each_port(&g_config, port) {
        rte_eth_stats_get(port->id, &st);
        ierr += st.ierrors;
        oerr += st.oerrors;
        imis += st.imissed;
    }

    net_stats_format_print_err(ierr, ierrors, STATS_BUF_LEN);
    net_stats_format_print_err(oerr, oerrors, STATS_BUF_LEN);
    net_stats_format_print_err(imis, imissed, STATS_BUF_LEN);
    if (fp) {
        fprintf(fp, "ierrors %s oerrors  %s imissed  %s\n\n", ierrors, oerrors, imissed);
    } else {
        printf("ierrors %s oerrors  %s imissed  %s\n\n", ierrors, oerrors, imissed);
    }
}

#define NET_STATS_BUF_LEN   (1024*8)
static char g_net_stats_buf[NET_STATS_BUF_LEN];

static void net_stats_output(FILE *fp, char *p)
{
    if (fp) {
        fwrite(p, 1, strlen(p), fp);
    } else {
        printf("%s", p);
    }
}

void net_stats_print_speed(FILE *fp, int seconds)
{
    char *p = g_net_stats_buf;
    int ret = 0;
    int len = NET_STATS_BUF_LEN;
    struct net_stats speed;

    if (g_config.quiet) {
        return;
    }

    SNPRINTF(p, len, "\nseconds %-18lu", (uint64_t)seconds);
    net_stats_get_speed(&speed);
    ret = net_stats_cpusage_print(p, len);
    buf_skip(p, len, ret);

    ret = net_stats_print(&speed, p, len);
    buf_skip(p, len, ret);
    net_stats_output(fp, g_net_stats_buf);
    net_stats_print_eth(fp);

    if (fp) {
        fflush(fp);
    }

err:
    return;
}

void net_stats_print_total(FILE *fp)
{
    char *p = g_net_stats_buf;
    int len = NET_STATS_BUF_LEN;
    int ret;
    struct net_stats sum;

    SNPRINTF(p, len, "\n");
    SNPRINTF(p, len, "-----------------------\n");
    SNPRINTF(p, len, "dperf Test Finished\n");
    SNPRINTF(p, len, "Version: %s\n", VERSION);
    SNPRINTF(p, len, "License: %s\n", "Apache 2.0");
    SNPRINTF(p, len, "Author:  %s\n", "Jianzhang Peng");

    SNPRINTF(p, len, "-----------------------\n");
    SNPRINTF(p, len, "\nTotal Numbers:\n");
    net_stats_sum(&sum);
    ret = net_stats_print(&sum, p, len);
    buf_skip(p, len, ret);
    net_stats_output(fp, g_net_stats_buf);
    net_stats_print_eth(fp);

    SNPRINTF(p, len, "-----------------------\n");
    if (fp) {
        fflush(fp);
    }

err:
    return;
}

void net_stats_timer_handler(struct work_space *ws)
{
    struct net_stats *s = &g_net_stats;
    s->cpusage = cpuload_cal_cpusage(&ws->load, ws->time.tsc);
}

void net_stats_init(struct work_space *ws)
{
    memset(&g_net_stats, 0, sizeof(struct net_stats));
    g_net_stats_all[ws->id] = &g_net_stats;
}

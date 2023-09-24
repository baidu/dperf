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

#include "mbuf_cache.h"

#include <string.h>

#include "udp.h"
#include "work_space.h"
#include "vxlan.h"

struct tcp_opt_mss {
    uint8_t kind;
    uint8_t len;
    uint16_t mss;
} __attribute__((__packed__));

static int mbuf_cache_init(struct mbuf_cache *pool, const char *name, struct work_space *ws, struct mbuf_data *mdata)
{
    if (ws->cfg->vxlan) {
        if (vxlan_encapsulate(mdata, ws) < 0) {
            return -1;
        }
    }

    pool->mbuf_pool = mbuf_pool_create(name, ws->port->id, ws->queue_id);
    if (pool->mbuf_pool == NULL) {
        return -1;
    }

    memcpy(&pool->data, mdata, sizeof(struct mbuf_data));
    return 0;
}

static struct tcphdr *mbuf_data_tcphdr(struct mbuf_data *mdata)
{
    return (struct tcphdr *)(mdata->data + mdata->l2_len + mdata->l3_len);
}

static struct udphdr *mbuf_data_uphdr(struct mbuf_data *mdata)
{
    return (struct udphdr*)mbuf_data_tcphdr(mdata);
}

static inline struct iphdr *mbuf_data_iphdr(struct mbuf_data *mdata)
{
    return (struct iphdr *)(mdata->data + mdata->l2_len);
}

static inline struct ip6_hdr *mbuf_data_ip6hdr(struct mbuf_data *mdata)
{
    return (struct ip6_hdr *)(mdata->data + mdata->l2_len);
}

static struct eth_hdr *mbuf_data_ethhdr(struct mbuf_data *mdata)
{
    return (struct eth_hdr *)(mdata->data);
}

static void mbuf_data_ip_tot_len_add(struct mbuf_data *mdata, uint16_t len)
{
    struct iphdr *iph = mbuf_data_iphdr(mdata);
    struct ip6_hdr *ip6h = mbuf_data_ip6hdr(mdata);

    if (mdata->ipv6) {
        ip6h->ip6_plen = htons((ntohs(ip6h->ip6_plen)) + len);
    } else {
        iph->tot_len = htons(ntohs(iph->tot_len) + len);
    }
}

static void mbuf_data_ip_set_proto(struct mbuf_data *mdata, uint8_t proto)
{
    struct iphdr *iph = mbuf_data_iphdr(mdata);
    struct ip6_hdr *ip6h = mbuf_data_ip6hdr(mdata);

    if (mdata->ipv6) {
        ip6h->ip6_nxt = proto;
    } else {
        iph->protocol = proto;
    }
}

static int mbuf_data_push(struct mbuf_data *mdata, const uint8_t *data, uint16_t len)
{
    uint8_t *p = NULL;

    if (mdata->total_len + len > MBUF_DATA_SIZE) {
        return -1;
    }

    p = &mdata->data[mdata->total_len];
    memcpy(p, data, len);
    mdata->total_len += len;
    return 0;
}

static int mbuf_data_push_l2(struct mbuf_data *mdata, struct netif_port *port)
{
    struct eth_hdr eth;
    uint16_t len = sizeof(struct eth_hdr);

    if (mdata->ipv6) {
        eth.type = htons(ETHER_TYPE_IPv6);
    } else {
        eth.type = htons(ETHER_TYPE_IPv4);
    }
    eth.d_addr = port->gateway_mac;
    eth.s_addr = port->local_mac;

    mdata->l2_len = len;
    return mbuf_data_push(mdata, (uint8_t*)&eth, len);
}

static int mbuf_data_push_ipv6(struct mbuf_data *data, struct work_space *ws)
{
    struct ip6_hdr ip6h;
    uint16_t len = sizeof(struct ip6_hdr);
    ipaddr_t sip;
    ipaddr_t dip;

    if (ws->cfg->server) {
        dip = ws->port->client_ip_range.start;
        sip = ws->port->server_ip_range.start;
    } else {
        sip = ws->port->client_ip_range.start;
        dip = ws->port->server_ip_range.start;
    }

    memset(&ip6h, 0, len);
    ip6h.ip6_vfc = (6 << 4);
    ip6h.ip6_flow |= htonl(((uint32_t)g_config.tos) << 20);
    ip6h.ip6_hops = DEFAULT_TTL;
    ip6h.ip6_src = sip.in6;
    ip6h.ip6_dst = dip.in6;
    /* ip6_nxt ip6_plen set later */

    data->l3_len = len;
    return mbuf_data_push(data, (uint8_t*)&ip6h, len);
}

static int mbuf_data_push_ipv4(struct mbuf_data *data)
{
    struct iphdr iph;
    uint16_t len = sizeof(struct iphdr);

    memset(&iph, 0, len);
    iph.ihl = 5;
    iph.version = 4;
    iph.tos = g_config.tos;
    iph.tot_len = htons(20);
    iph.ttl = DEFAULT_TTL;
    iph.frag_off = IP_FLAG_DF;
    /* protocol: set later*/

    data->l3_len = len;
    return mbuf_data_push(data, (uint8_t*)&iph, len);
}

static int mbuf_data_push_ip(struct mbuf_data *mdata, struct work_space *ws)
{
    if (mdata->ipv6) {
        return mbuf_data_push_ipv6(mdata, ws);
    } else {
        return mbuf_data_push_ipv4(mdata);
    }
}

static int mbuf_data_push_tcp(struct mbuf_data *mdata)
{
    struct tcphdr th;
    uint16_t len = sizeof(struct tcphdr);

    memset(&th, 0, len);
    th.th_win = htons(TCP_WIN);
    th.th_off = 5;

    mdata->l4_len = len;
    if (mbuf_data_push(mdata, (uint8_t*)&th, len) != 0) {
        return -1;
    }

    mbuf_data_ip_set_proto(mdata, IPPROTO_TCP);
    mbuf_data_ip_tot_len_add(mdata, mdata->l4_len);
    return 0;
}

static int mbuf_data_push_tcp_opt(struct mbuf_data *mdata, void *data, int len)
{
    struct tcphdr *th = NULL;

    if ((len % 4) != 0) {
        return -1;
    }
    th = mbuf_data_tcphdr(mdata);
    th->th_off++;
    mdata->l4_len += len;
    if (mbuf_data_push(mdata, (uint8_t*)data, len) != 0) {
        return -1;
    }

    mbuf_data_ip_tot_len_add(mdata, len);

    return 0;
}

static int mbuf_data_push_tcp_mss(struct mbuf_data *mdata, uint16_t mss)
{
    struct tcp_opt_mss opt_mss;

    if (mss == 0) {
        return 0;
    }
    opt_mss.kind = 2;
    opt_mss.len = 4;
    opt_mss.mss = htons(mss);

    return mbuf_data_push_tcp_opt(mdata, (void *)&opt_mss, sizeof(struct tcp_opt_mss));
}

static int mbuf_data_push_tcp_wscale(struct mbuf_data *mdata)
{
    /*
     * nop
     * kind = 3, len = 3, wscale
     * */
    uint8_t wscale[4] = {1, 3, 3, DEFAULT_WSCALE};

    return mbuf_data_push_tcp_opt(mdata, (void *)wscale, 4);
}

static int mbuf_data_push_udp(struct mbuf_data *mdata)
{
    struct udphdr uh;
    uint16_t len = sizeof(struct udphdr);

    memset(&uh, 0, len);
    mdata->l4_len = len;
    uh.len = htons(len);
    mbuf_data_ip_set_proto(mdata, IPPROTO_UDP);
    mbuf_data_ip_tot_len_add(mdata, mdata->l4_len);

    return mbuf_data_push(mdata, (uint8_t*)&uh, len);
}

static int mbuf_data_push_data(struct mbuf_data *mdata, const char *data)
{
    uint16_t len = 0;

    if (data == NULL) {
        return 0;
    }

    len = strlen(data);
    if (len == 0) {
        return 0;
    }

    if (mbuf_data_push(mdata, (const uint8_t*)data, len) != 0) {
        return -1;
    }

    mdata->data_len = len;
    mbuf_data_ip_tot_len_add(mdata, len);

    return 0;
}

int mbuf_cache_init_tcp(struct mbuf_cache *cache, struct work_space *ws, const char *name, uint16_t mss, const char *data)
{
    struct mbuf_data mdata;

    memset(&mdata, 0, sizeof(struct mbuf_data));
    mdata.ipv6 = ws->ipv6;
    if (mbuf_data_push_l2(&mdata, ws->port) < 0) {
        return -1;
    }

    if (mbuf_data_push_ip(&mdata, ws) < 0) {
        return -1;
    }

    if (mbuf_data_push_tcp(&mdata) < 0) {
        return -1;
    }

    if (mss > 0) {
        if (mbuf_data_push_tcp_mss(&mdata, mss) < 0) {
            return -1;
        }

        if (mbuf_data_push_tcp_wscale(&mdata) < 0) {
            return -1;
        }
    }

    if (mbuf_data_push_data(&mdata, data) < 0) {
        return -1;
    }

    return mbuf_cache_init(cache, name, ws, &mdata);
}

int mbuf_cache_init_udp(struct mbuf_cache *cache, struct work_space *ws, const char *name, const char *data)
{
    struct udphdr *uh = NULL;
    struct mbuf_data mdata;

    memset(&mdata, 0, sizeof(struct mbuf_data));
    mdata.ipv6 = ws->ipv6;
    if (mbuf_data_push_l2(&mdata, ws->port) < 0) {
        return -1;
    }

    if (mbuf_data_push_ip(&mdata, ws) < 0) {
        return -1;
    }

    if (mbuf_data_push_udp(&mdata) < 0) {
        return -1;
    }

    if (mbuf_data_push_data(&mdata, data) < 0) {
        return -1;
    }

    uh = mbuf_data_uphdr(&mdata);
    uh->len = htons(mdata.l4_len + mdata.data_len);

    return mbuf_cache_init(cache, name, ws, &mdata);
}

void mbuf_cache_set_dmac(struct mbuf_cache *cache, struct eth_addr *ea)
{
    struct eth_hdr *eth = NULL;

    if (cache->data.l2_len == 0) {
        return;
    }

    eth = mbuf_data_ethhdr(&cache->data);
    eth_addr_copy(&eth->d_addr, ea);
}

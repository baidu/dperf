/*
 * Copyright (c) 2022 Baidu.com, Inc. All Rights Reserved.
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

#include "vxlan.h"
#include "mbuf_cache.h"
#include "work_space.h"
#include "port.h"

static void vxlan_set_innter_eth_hdr(struct work_space *ws, struct mbuf_data *mdata)
{
    struct vxlan *vxlan = NULL;
    struct eth_hdr *eth = NULL;

    vxlan = ws->port->vxlan;
    eth = (struct eth_hdr *)(mdata->data);
    eth->d_addr = vxlan->inner_dmac;
    eth->s_addr = vxlan->inner_smac;
}

static int vxlan_push_headers(struct mbuf_data *mdata, struct vxlan_headers *vxhs)
{
    int len = sizeof(struct vxlan_headers);

    if ((mdata->total_len + len) > MBUF_DATA_SIZE) {
        return -1;
    }

    memmove(mdata->data + len, mdata->data, mdata->total_len);
    memcpy(mdata->data, vxhs, len);
    mdata->total_len += len;
    mdata->vxlan = true;

    return 0;
}

static void vxlan_set_eth_hdr(struct work_space *ws, struct eth_hdr *eh)
{
    struct netif_port *port = NULL;

    port = ws->port;
    /* set outer l2 header */
    eh->type = htons(ETHER_TYPE_IPv4);
    eh->d_addr = port->gateway_mac;
    eh->s_addr = port->local_mac;
}

static void vxlan_set_iphdr(struct work_space *ws, struct iphdr *iph, uint16_t inner_len)
{
    uint16_t tot_len = 0;
    uint32_t saddr = 0;
    uint32_t daddr = 0;
    struct vxlan *vxlan = ws->port->vxlan;

    saddr = ip_range_get(&vxlan->vtep_local, ws->queue_id);
    if (vxlan->vtep_remote.num > 1) {
        daddr = ip_range_get(&vxlan->vtep_remote, ws->queue_id);
    } else {
        daddr = ip_range_get(&vxlan->vtep_remote, 0);
    }

    tot_len = sizeof(struct iphdr) +
              sizeof(struct udphdr) +
              sizeof(struct vxlan_header) +
              inner_len;

    iph->ihl = 5;
    iph->version = 4;
    iph->tos = g_config.tos;
    iph->tot_len = htons(tot_len);
    iph->ttl = DEFAULT_TTL;
    iph->protocol = IPPROTO_UDP;
    iph->frag_off = IP_FLAG_DF;

    iph->saddr = saddr;
    iph->daddr = daddr;
    iph->check = RTE_IPV4_CKSUM(iph);
}

static void vxlan_set_udphdr(struct work_space *ws, struct udphdr *uh, uint16_t inner_len)
{
    uint16_t len = 0;

    len = sizeof(struct udphdr) + sizeof(struct vxlan_header) + inner_len;
    uh->dest = htons(VXLAN_PORT);
    uh->source = htons(VXLAN_SPORT + ws->id);
    uh->len = htons(len);
}

int vxlan_encapsulate(struct mbuf_data *mdata, struct work_space *ws)
{
    uint16_t inner_len = 0;
    struct vxlan *vxlan = NULL;
    struct vxlan_headers vxhs;

    vxlan = ws->port->vxlan;
    inner_len = mdata->total_len;
    memset(&vxhs, 0, sizeof(struct vxlan_headers));

    vxlan_set_innter_eth_hdr(ws, mdata);
    vxlan_set_eth_hdr(ws, &vxhs.eh);
    vxlan_set_iphdr(ws, &vxhs.iph, inner_len);
    vxlan_set_udphdr(ws, &vxhs.uh, inner_len);
    vxhs.vxh.vni = VXLAN_HTON(vxlan->vni);
    vxhs.vxh.flags = 0x08;  /* vni valid */

    return vxlan_push_headers(mdata, &vxhs);
}

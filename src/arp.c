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

#include "arp.h"

#include <unistd.h>
#include <rte_arp.h>
#include <rte_ethdev.h>

#include "config.h"
#include "mbuf.h"
#include "port.h"
#include "work_space.h"
#include "kni.h"
#include "bond.h"

static struct eth_addr g_mac_zero = {.bytes = {0, 0, 0, 0, 0, 0}};
static struct eth_addr g_mac_full = {.bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff}};

static inline void arp_set_arphdr(struct arphdr *arp, uint16_t op, uint32_t sip, uint32_t dip,
    const struct eth_addr *sa, const struct eth_addr *da)
{
    arp->ar_hrd = htons(0x01);
    arp->ar_pro = htons(0x0800);
    arp->ar_hln = 0x06;
    arp->ar_pln = 0x04;
    arp->ar_op = htons(op);
    arp->ar_sip = sip;
    arp->ar_tip = dip;
    eth_addr_copy(&arp->ar_sha, sa);
    eth_addr_copy(&arp->ar_tha, da);
}

void arp_send(struct work_space *ws, struct rte_mbuf *m)
{
    if (port_is_bond4(ws->port)) {
        bond_broadcast(ws, m);
    }

    work_space_tx_send(ws, m);
    net_stats_arp_tx();
}

static void arp_set_request(struct rte_mbuf *m, struct eth_addr *smac, uint32_t dip, uint32_t sip)
{
    struct eth_hdr *eth = NULL;
    struct arphdr *arp = NULL;

    eth = mbuf_push_eth_hdr(m);
    arp = mbuf_push_arphdr(m);

    eth_hdr_set(eth, ETHER_TYPE_ARP, &g_mac_full, smac);
    arp_set_arphdr(arp, ARP_REQUEST, sip, dip, smac, &g_mac_zero);
}

static void arp_request_gw2(struct work_space *ws, uint32_t local_ip)
{
    struct rte_mbuf *m = NULL;
    struct netif_port *port = ws->port;

    m = work_space_alloc_mbuf(ws);
    if (m == NULL) {
        return;
    }

    arp_set_request(m, &port->local_mac, port->gateway_ip.ip, local_ip);
    arp_send(ws, m);
}

void arp_request_gw(struct work_space *ws)
{
    int i = 0;
    uint32_t ip = 0;
    struct netif_port *port = NULL;
    struct ip_range *ip_range = NULL;
    struct vxlan *vxlan = NULL;

    port = ws->port;
    if (port->ipv6) {
        return;
    }

    if (ws->vxlan) {
        vxlan = port->vxlan;
        ip_range = &vxlan->vtep_local;
    } else {
        ip_range = port->local_ip_range;
    }
    arp_request_gw2(ws, port->local_ip.ip);
    for (i = 0; i < ip_range->num; i++) {
        ip = ip_range_get(ip_range, i);
        arp_request_gw2(ws, ip);
    }
}

static void arp_reply(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct arphdr *arph = mbuf_arphdr(m);
    uint32_t sip = arph->ar_tip;
    uint32_t dip = arph->ar_sip;
    struct netif_port *port = ws->port;
    const struct eth_addr *smac = &port->local_mac;
    struct eth_addr dmac;

    eth_addr_copy(&dmac, &eth->s_addr);
    eth_hdr_set(eth, ETHER_TYPE_ARP, &dmac, smac);
    arp_set_arphdr(arph, ARP_REPLY, sip, dip, smac, &dmac);
    arp_send(ws, m);
}

static void arp_process_reply(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct arphdr *arph = mbuf_arphdr(m);
    struct netif_port *port = ws->port;

    if ((port->gateway_ip.ip == arph->ar_sip) && eth_addr_is_zero(&port->gateway_mac)) {
        work_space_update_gw(ws, &eth->s_addr);
    }

    mbuf_free(m);
}

static void arp_process_request(struct work_space *ws, struct rte_mbuf *m)
{
    uint32_t dip = 0;
    struct arphdr *arph = NULL;

    arph = mbuf_arphdr(m);
    dip = arph->ar_tip;

    if (!work_space_ip_exist(ws, dip)) {
        mbuf_free(m);
        return;
    }

    arp_reply(ws, m);
}

void arp_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct arphdr *arph = NULL;

    net_stats_arp_rx();
    arph = mbuf_arphdr(m);

    if (ws->kni) {
        kni_broadcast(ws, m);
    }

    if (arph->ar_op == htons(ARP_REQUEST)) {
        arp_process_request(ws, m);
    } else if (arph->ar_op == htons(ARP_REPLY)) {
        arp_process_reply(ws, m);
    } else {
        mbuf_free(m);
    }
}

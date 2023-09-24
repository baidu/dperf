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

#include "icmp6.h"
#include "mbuf.h"
#include "work_space.h"
#include "kni.h"
#include "bond.h"

static void icmp6_send(struct work_space *ws, struct rte_mbuf *m);

struct icmp6_nd_opt {
    uint8_t  type;
    uint8_t  len;
    struct eth_addr mac;
} __attribute__((__packed__));

static void icmp6_nd_opt_set(struct icmp6_nd_opt *opt, uint8_t type, const struct eth_addr *mac)
{
    opt->type = type;
    opt->len = 1;
    eth_addr_copy(&opt->mac, mac);
}

static void icmp6_nd_na_set(struct nd_neighbor_advert *na, const struct eth_addr *smac)
{
    struct icmp6_nd_opt *opt = NULL;

    na->nd_na_type = ND_NEIGHBOR_ADVERT;
    na->nd_na_code = 0;
    na->nd_na_cksum = 0;
    na->nd_na_flags_reserved = ND_NA_FLAG_SOLICITED | ND_NA_FLAG_OVERRIDE;

    opt = (struct icmp6_nd_opt *)((uint8_t*)na + sizeof(struct nd_neighbor_advert));
    icmp6_nd_opt_set(opt, ND_OPT_TARGET_LINKADDR, smac);
}

static void icmp6_ns_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct ip6_hdr *ip6h = mbuf_ip6_hdr(m);
    struct icmp6_hdr *icmp6h = mbuf_icmp6_hdr(m);
    const struct eth_addr *smac = &(ws->port->local_mac);
    struct nd_neighbor_solicit *ns = (struct nd_neighbor_solicit *)icmp6h;

    net_stats_icmp_rx();
    if ((!work_space_ip6_exist(ws, (ipaddr_t *)&(ns->nd_ns_target)))
            || (ip6h->ip6_hops != ND_TTL)) {
        mbuf_free(m);
        return;
    }

    kni_broadcast(ws, m);
    eth_addr_copy(&eth->d_addr, &eth->s_addr);
    eth_addr_copy(&eth->s_addr, smac);

    ip6h->ip6_dst = ip6h->ip6_src;
    ip6h->ip6_src = ns->nd_ns_target;

    icmp6_nd_na_set((struct nd_neighbor_advert *)icmp6h, smac);

    icmp6h->icmp6_cksum = 0;
    icmp6h->icmp6_cksum = RTE_IPV6_UDPTCP_CKSUM(ip6h, icmp6h);
    icmp6_send(ws, m);
}

static void icmp6_na_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct netif_port *port = ws->port;
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct ip6_hdr *ip6h = mbuf_ip6_hdr(m);

    if (ipaddr_eq(&port->gateway_ip, &ip6h->ip6_src)
            && eth_addr_is_zero(&port->gateway_mac)) {
        work_space_update_gw(ws, &eth->s_addr);
    }

    if (ws->kni) {
        return kni_recv(ws, m);
    }

    mbuf_free(m);
}

static void icmp6_echo_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct eth_hdr *eth = mbuf_eth_hdr(m);
    struct ip6_hdr *ip6h = mbuf_ip6_hdr(m);
    struct icmp6_hdr *icmp6h = mbuf_icmp6_hdr(m);
    const struct eth_addr *smac = &(ws->port->local_mac);

    if (!work_space_ip6_exist(ws, (ipaddr_t *)&(ip6h->ip6_dst))) {
        mbuf_free(m);
        return;
    }

    eth_addr_copy(&eth->d_addr, &eth->s_addr);
    eth_addr_copy(&eth->s_addr, smac);

    ip6h_swap_addr(ip6h);
    ip6h->ip6_flow |= htonl(((uint32_t)g_config.tos) << 20);
    icmp6h->icmp6_type = ICMP6_ECHO_REPLY;
    icmp6h->icmp6_code = 0;

    icmp6h->icmp6_cksum = 0;
    icmp6h->icmp6_cksum = RTE_IPV6_UDPTCP_CKSUM(ip6h, icmp6h);

    icmp6_send(ws, m);
}

void icmp6_process(struct work_space *ws, struct rte_mbuf *m)
{
    struct icmp6_hdr *icmp6h = mbuf_icmp6_hdr(m);
    uint8_t type = icmp6h->icmp6_type;
    uint8_t code = icmp6h->icmp6_code;

    net_stats_icmp_rx();
    if (code != 0) {
        mbuf_free(m);
        return;
    }

    if (type == ICMP6_ECHO_REQUEST) {
        icmp6_echo_process(ws, m);
    } else if (type == ND_NEIGHBOR_SOLICIT) {
        icmp6_ns_process(ws, m);
    } else if (type == ND_NEIGHBOR_ADVERT) {
        icmp6_na_process(ws, m);
    } else if (type == ICMP6_ECHO_REPLY) {
        if (ws->kni && work_space_is_local_addr(ws, m)) {
            return kni_recv(ws, m);
        }
        mbuf_free(m);
    } else {
        mbuf_free(m);
    }
}

static void icmp6_multicast_ip6_addr(const struct in6_addr *unicast, struct in6_addr *multicast)
{
    memset(multicast, 0, sizeof(struct in6_addr));
    multicast->s6_addr[0] = 0xff;
    multicast->s6_addr[1] = 0x02;
    multicast->s6_addr[11] = 0x01;
    multicast->s6_addr[12] = 0xff;
    multicast->s6_addr[13] = unicast->s6_addr[13];
    multicast->s6_addr[14] = unicast->s6_addr[14];
    multicast->s6_addr[15] = unicast->s6_addr[15];
}

static void icmp6_multicast_ether_addr(const struct in6_addr *daddr, struct eth_addr *mac)
{
    struct in6_addr mcast_addr;

    mac->bytes[0] = 0x33;
    mac->bytes[1] = 0x33;
    icmp6_multicast_ip6_addr(daddr, &mcast_addr);
    memcpy(mac->bytes + 2, &mcast_addr.s6_addr32[3], sizeof(uint32_t));
}

static void icmp6_ns_eth_hdr_push(const struct netif_port *port, struct rte_mbuf *m)
{
    struct eth_addr dmac;
    struct eth_hdr *eth = NULL;

    icmp6_multicast_ether_addr(&port->gateway_ip.in6, &dmac);

    eth = mbuf_push_eth_hdr(m);
    eth_hdr_set(eth, ETHER_TYPE_IPv6, &dmac, &(port->local_mac));
}

static void icmp6_ns_ip6_hdr_push(const struct netif_port *port, struct rte_mbuf *m)
{
    uint16_t plen = 0;
    struct ip6_hdr *ip6h = NULL;

    plen = sizeof(struct nd_neighbor_solicit) + sizeof(struct icmp6_nd_opt);

    ip6h = mbuf_push_ip6_hdr(m);
    memset(ip6h, 0, sizeof(struct ip6_hdr));
    ip6h->ip6_vfc = (6 << 4);
    ip6h->ip6_hops = ND_TTL;
    ip6h->ip6_src = port->local_ip.in6;
    ip6h->ip6_plen = htons(plen);
    ip6h->ip6_nxt = IPPROTO_ICMPV6;

    icmp6_multicast_ip6_addr(&port->gateway_ip.in6, &ip6h->ip6_dst);
}

static void icmp6_ns_hdr_push(const struct netif_port *port, struct rte_mbuf *m)
{
    struct ip6_hdr *ip6h = NULL;
    struct icmp6_nd_opt *opt = NULL;
    struct nd_neighbor_solicit *ns = NULL;

    ip6h = mbuf_ip6_hdr(m);
    ns = RTE_PKTMBUF_PUSH(m, struct nd_neighbor_solicit);
    memset(ns, 0, sizeof(struct nd_neighbor_solicit));
    ns->nd_ns_type = ND_NEIGHBOR_SOLICIT;
    ns->nd_ns_target = port->gateway_ip.in6;

    opt = RTE_PKTMBUF_PUSH(m, struct icmp6_nd_opt);
    icmp6_nd_opt_set(opt, ND_OPT_SOURCE_LINKADDR, &(port->local_mac));

    ns->nd_ns_cksum = 0;
    ns->nd_ns_cksum = RTE_IPV6_UDPTCP_CKSUM(ip6h, ns);
}

/*
 * request gateway's MAC address
 * */
void icmp6_ns_request(struct work_space *ws)
{
    struct rte_mbuf *m = NULL;

    if (!ws->ipv6) {
        return;
    }

    m = work_space_alloc_mbuf(ws);
    if (m == NULL) {
        return;
    }

    icmp6_ns_eth_hdr_push(ws->port, m);
    icmp6_ns_ip6_hdr_push(ws->port, m);
    icmp6_ns_hdr_push(ws->port, m);
    icmp6_send(ws, m);
}

static void icmp6_send(struct work_space *ws, struct rte_mbuf *m)
{
    if (icmp6_is_neigh(m) && port_is_bond4(ws->port)) {
        bond_broadcast(ws, m);
    }

    net_stats_icmp_tx();
    work_space_tx_send(ws, m);
}

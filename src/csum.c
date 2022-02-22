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

#include "socket.h"
#include "work_space.h"
#include "config.h"
#include <stdlib.h>
#include <netinet/ip.h>
#include "tcp.h"
#include "udp.h"

#define L4_DATA_LEN(mcache) ((mcache)->data.l4_len + (mcache)->data.data_len)

static inline uint16_t csum_pseudo_ipv4(uint8_t proto, uint32_t sip, uint32_t dip, uint16_t len)
{
    uint32_t csum = 0;

    csum = (sip & 0x0000ffffUL) + (sip >> 16);
    csum += (dip & 0x0000ffffUL) + (dip >> 16);

    csum += (uint16_t)proto << 8;
    csum += htons(len);

    csum = (csum & 0x0000ffffUL) + (csum >> 16);
    csum = (csum & 0x0000ffffUL) + (csum >> 16);

    return (uint16_t)csum;
}

static inline uint16_t csum_pseudo_ipv6(uint8_t proto, struct in6_addr *saddr, struct in6_addr *daddr,
    uint16_t len)
{
    struct {
        struct in6_addr saddr;
        struct in6_addr daddr;
        uint8_t zero;
        uint8_t proto;
        uint16_t len;
    } hdr;

    hdr.saddr = *saddr;
    hdr.daddr = *daddr;
    hdr.zero = 0;
    hdr.proto = proto;
    hdr.len = htons(len);

    return rte_raw_cksum((void *)&hdr, sizeof(hdr));
}

static inline void csum_init_pseudo_ipv6(struct socket *sk, struct work_space *ws, uint8_t proto)
{
    struct in6_addr laddr;
    struct in6_addr faddr;
    struct ip6_hdr *ip6h = NULL;
    struct mbuf_data *mdata = NULL;

    mdata = &ws->tcp.data;
    ip6h = (struct ip6_hdr *)((uint8_t*)(mdata->data) + sizeof(struct eth_hdr));
    laddr = ip6h->ip6_src;
    faddr = ip6h->ip6_dst;
    laddr.s6_addr32[3] = sk->laddr;
    faddr.s6_addr32[3] = sk->faddr;

    if (proto == IPPROTO_TCP) {
        sk->csum_tcp = csum_pseudo_ipv6(proto, &laddr, &faddr, L4_DATA_LEN(&ws->tcp));
        sk->csum_tcp_opt = csum_pseudo_ipv6(proto, &laddr, &faddr, L4_DATA_LEN(&ws->tcp_opt));
        sk->csum_tcp_data = csum_pseudo_ipv6(proto, &laddr, &faddr, L4_DATA_LEN(&ws->tcp_data));
    } else {
        sk->csum_udp = csum_pseudo_ipv6(proto, &laddr, &faddr, L4_DATA_LEN(&ws->udp));
    }
}

static inline void csum_init_pseudo_ipv4(struct socket *sk, struct work_space *ws, uint8_t proto,
    uint32_t lip, uint32_t fip)
{
    if (proto == IPPROTO_TCP) {
        sk->csum_tcp = csum_pseudo_ipv4(proto, lip, fip, L4_DATA_LEN(&ws->tcp));
        sk->csum_tcp_opt = csum_pseudo_ipv4(proto, lip, fip, L4_DATA_LEN(&ws->tcp_opt));
        sk->csum_tcp_data = csum_pseudo_ipv4(proto, lip, fip, L4_DATA_LEN(&ws->tcp_data));
    } else {
        sk->csum_udp = csum_pseudo_ipv4(proto, lip, fip, L4_DATA_LEN(&ws->udp));
    }
}

static void csum_init_pseudo(struct work_space *ws, struct socket *sk)
{
    uint8_t proto = ws->cfg->protocol;

    if (ws->ipv6) {
        csum_init_pseudo_ipv6(sk, ws, proto);
    } else {
        csum_init_pseudo_ipv4(sk, ws, proto, sk->laddr, sk->faddr);
    }
}

static void csum_inner_tcp_udp_ipv4(struct socket *sk, struct mbuf_cache *mcache, uint16_t *ip_csum, uint16_t *tcp_csum)
{
    struct iphdr *iph = NULL;
    struct tcphdr *th = NULL;
    struct mbuf_data *mdata = NULL;

    mdata = &mcache->data;
    iph = (struct iphdr *)((uint8_t*)(mdata->data) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
    th = (struct tcphdr *)(((uint8_t *)iph) + sizeof(struct iphdr));

    iph->saddr = sk->laddr;
    iph->daddr = sk->faddr;
    th->th_sport = sk->lport;
    th->th_dport = sk->fport;

    *ip_csum = RTE_IPV4_CKSUM(iph);
    *tcp_csum = RTE_IPV4_UDPTCP_CKSUM(iph, th);

    iph->saddr = 0;
    iph->daddr = 0;
    th->th_sport = 0;
    th->th_dport = 0;
}

static void csum_init_inner_ipv4(struct work_space *ws, struct socket *sk)
{
    if (ws->cfg->protocol == IPPROTO_TCP) {
        csum_inner_tcp_udp_ipv4(sk, &ws->tcp, &sk->csum_ip, &sk->csum_tcp);
        csum_inner_tcp_udp_ipv4(sk, &ws->tcp_opt, &sk->csum_ip_opt, &sk->csum_tcp_opt);
        csum_inner_tcp_udp_ipv4(sk, &ws->tcp_data, &sk->csum_ip_data, &sk->csum_tcp_data);
    } else {
        csum_inner_tcp_udp_ipv4(sk, &ws->udp, &sk->csum_ip, &sk->csum_udp);
    }
}

static void csum_inner_tcp_udp_tcp_ipv6(struct socket *sk, struct mbuf_cache *mcache, uint16_t *tcp_csum)
{
    struct ip6_hdr *ip6h = NULL;
    struct tcphdr *th = NULL;
    struct mbuf_data *mdata = NULL;

    mdata = &mcache->data;
    ip6h = (struct ip6_hdr *)((uint8_t*)(mdata->data) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
    ip6h->ip6_src.s6_addr32[3] = sk->laddr;
    ip6h->ip6_dst.s6_addr32[3] = sk->faddr;
    th = (struct tcphdr *)(((uint8_t *)ip6h) + sizeof(struct ip6_hdr));
    th->th_sport = sk->lport;
    th->th_dport = sk->fport;
    *tcp_csum = RTE_IPV6_UDPTCP_CKSUM(ip6h, th);
    ip6h->ip6_src.s6_addr32[3] = 0;
    ip6h->ip6_dst.s6_addr32[3] = 0;
    th->th_sport = 0;
    th->th_dport = 0;
}

static void csum_init_inner_ipv6(struct work_space *ws, struct socket *sk)
{
    if (ws->cfg->protocol == IPPROTO_TCP) {
        csum_inner_tcp_udp_tcp_ipv6(sk, &ws->tcp, &sk->csum_tcp);
        csum_inner_tcp_udp_tcp_ipv6(sk, &ws->tcp_opt, &sk->csum_tcp_opt);
        csum_inner_tcp_udp_tcp_ipv6(sk, &ws->tcp_data, &sk->csum_tcp_data);
    } else {
        csum_inner_tcp_udp_tcp_ipv6(sk, &ws->udp, &sk->csum_udp);
    }
}

static void csum_init_inner(struct work_space *ws, struct socket *sk)
{
    if (ws->ipv6) {
        csum_init_inner_ipv6(ws, sk);
    } else {
        csum_init_inner_ipv4(ws, sk);
    }
}

void csum_init_socket(struct work_space *ws, struct socket *sk)
{
    /*
     * 1. for the inner packet's checksum, many network interface cannot offload it,
     *    we use the incremental algorithm to calculate the checksum.
     * 2. But most hardware can offload underlay packet's checksum.
     */
    if (ws->vxlan) {
        csum_init_inner(ws, sk);
    } else {
        csum_init_pseudo(ws, sk);
    }
}

static int csum_check_ipv4(struct iphdr *iph)
{
    struct tcphdr *th = NULL;
    struct udphdr *uh = NULL;
    uint16_t csum_ip = 0;
    uint16_t csum_tcp = 0;
    uint16_t csum_udp = 0;

    th = (struct tcphdr *)((uint8_t *)iph + sizeof(struct iphdr));
    uh = (struct udphdr *)th;
    csum_ip = iph->check;
    iph->check = 0;

    if (csum_ip != RTE_IPV4_CKSUM(iph)) {
        printf("csum ip error\n");
        return -1;
    }
    if (iph->protocol == IPPROTO_TCP) {
        csum_tcp = th->th_sum;
        th->th_sum = 0;
        if (csum_tcp != RTE_IPV4_UDPTCP_CKSUM(iph, th)) {
            printf("csum tcp error\n");
            return -1;
        }
        th->th_sum = csum_tcp;
    } else {
        csum_udp = uh->check;
        uh->check = 0;
        if (csum_udp != RTE_IPV4_UDPTCP_CKSUM(iph, uh)) {
            printf("csum udp error\n");
            return -1;
        }
        uh->check = 0;
    }
    iph->check = csum_ip;

    return 0;
}

static int csum_check_ipv6(struct ip6_hdr *ip6h)
{
    struct tcphdr *th = NULL;
    struct udphdr *uh = NULL;
    uint16_t csum_tcp = 0;
    uint16_t csum_udp = 0;

    th = (struct tcphdr *)((uint8_t *)ip6h + sizeof(struct ip6_hdr));
    uh = (struct udphdr *)th;
    if (ip6h->ip6_nxt == IPPROTO_TCP) {
        csum_tcp = th->th_sum;
        th->th_sum = 0;
        if (csum_tcp != RTE_IPV6_UDPTCP_CKSUM(ip6h, th)) {
            printf("csum tcp error\n");
            return -1;
        }
    } else {
        csum_udp = uh->check;
        uh->check = 0;
        if (csum_udp != RTE_IPV6_UDPTCP_CKSUM(ip6h, uh)) {
            printf("csum udp error\n");
            return -1;
        }
        uh->check = csum_udp;
    }

    return 0;
}

int csum_check(struct rte_mbuf *m)
{
    struct iphdr *iph = NULL;

    iph = (struct iphdr *)((uint8_t *)mbuf_eth_hdr(m) + VXLAN_HEADERS_SIZE + sizeof(struct eth_hdr));
    if (iph->version == 4) {
        return csum_check_ipv4(iph);
    } else {
        return csum_check_ipv6((struct ip6_hdr *)iph);
    }

    return 0;
}

/*
 * Copyright (c) 2021-2022 Baidu.com, Inc. All Rights Reserved.
 * Copyright (c) 2022-2024 Jianzhang Peng. All Rights Reserved.
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

#ifndef __DPDK_H
#define __DPDK_H

#include <rte_version.h>

#if RTE_VERSION < RTE_VERSION_NUM(21, 0, 0, 0)
#define RTE_ETH_MQ_RX_NONE              ETH_MQ_RX_NONE
#define RTE_ETH_MQ_TX_NONE              ETH_MQ_TX_NONE

#define RTE_ETH_TX_OFFLOAD_IPV4_CKSUM   DEV_TX_OFFLOAD_IPV4_CKSUM
#define RTE_ETH_TX_OFFLOAD_TCP_CKSUM    DEV_TX_OFFLOAD_TCP_CKSUM
#define RTE_ETH_TX_OFFLOAD_UDP_CKSUM    DEV_TX_OFFLOAD_UDP_CKSUM
#define RTE_ETH_TX_OFFLOAD_VLAN_INSERT  DEV_TX_OFFLOAD_VLAN_INSERT
#define RTE_ETH_RX_OFFLOAD_VLAN_STRIP   DEV_RX_OFFLOAD_VLAN_STRIP

#define RTE_ETH_RSS_IPV4                ETH_RSS_IPV4
#define RTE_ETH_RSS_FRAG_IPV4           ETH_RSS_FRAG_IPV4
#define RTE_ETH_RSS_IPV6                ETH_RSS_IPV6
#define RTE_ETH_RSS_FRAG_IPV6           ETH_RSS_FRAG_IPV6

#define RTE_ETH_RSS_NONFRAG_IPV4_UDP    ETH_RSS_NONFRAG_IPV4_UDP
#define RTE_ETH_RSS_NONFRAG_IPV6_UDP    ETH_RSS_NONFRAG_IPV6_UDP
#define RTE_ETH_RSS_NONFRAG_IPV4_TCP    ETH_RSS_NONFRAG_IPV4_TCP
#define RTE_ETH_RSS_NONFRAG_IPV6_TCP    ETH_RSS_NONFRAG_IPV6_TCP

#define RTE_ETH_MQ_RX_RSS               ETH_MQ_RX_RSS
#endif

#if RTE_VERSION < RTE_VERSION_NUM(21, 0, 0, 0)
#define RTE_MBUF_F_RX_L4_CKSUM_BAD  PKT_RX_L4_CKSUM_BAD
#define RTE_MBUF_F_RX_IP_CKSUM_BAD  PKT_RX_IP_CKSUM_BAD
#define RTE_MBUF_F_TX_IPV6          PKT_TX_IPV6
#define RTE_MBUF_F_TX_IP_CKSUM      PKT_TX_IP_CKSUM
#define RTE_MBUF_F_TX_IPV4          PKT_TX_IPV4
#define RTE_MBUF_F_TX_TCP_CKSUM     PKT_TX_TCP_CKSUM
#define RTE_MBUF_F_TX_UDP_CKSUM     PKT_TX_UDP_CKSUM
#endif

#if RTE_VERSION >= RTE_VERSION_NUM(19, 0, 0, 0)
#include <net/ethernet.h>

#define ETHER_TYPE_IPv4 ETHERTYPE_IP
#define ETHER_TYPE_IPv6 ETHERTYPE_IPV6
#define ETHER_TYPE_ARP  ETHERTYPE_ARP

#define RTE_ETH_MACADDR_GET(port_id, mac_addr) rte_eth_macaddr_get(port_id, (struct rte_ether_addr *)mac_addr)
#else
#define RTE_ETH_MACADDR_GET(port_id, mac_addr) rte_eth_macaddr_get(port_id, (struct ether_addr *)mac_addr)
#endif

#if RTE_VERSION < RTE_VERSION_NUM(19, 0, 0, 0)
#define RTE_IPV4_CKSUM(iph) rte_ipv4_cksum((struct ipv4_hdr*)iph)
#define RTE_IPV4_UDPTCP_CKSUM(iph, th) rte_ipv4_udptcp_cksum((const struct ipv4_hdr *)iph, th)
#define RTE_IPV6_UDPTCP_CKSUM(iph, th) rte_ipv6_udptcp_cksum((const struct ipv6_hdr *)iph, (const void *)th)
#else
#define RTE_IPV4_CKSUM(iph) rte_ipv4_cksum((const struct rte_ipv4_hdr *)iph)
#define RTE_IPV4_UDPTCP_CKSUM(iph, th) rte_ipv4_udptcp_cksum((const struct rte_ipv4_hdr *)iph, th)
#define RTE_IPV6_UDPTCP_CKSUM(iph, th) rte_ipv6_udptcp_cksum((const struct rte_ipv6_hdr *)iph, (const void *)th)
#endif

#if RTE_VERSION < RTE_VERSION_NUM(21, 11, 0, 0)
#define RTE_MBUF_F_TX_VLAN  PKT_TX_VLAN
#endif

#if RTE_VERSION < RTE_VERSION_NUM(23, 0, 0, 0)
#define KNI_ENABLE
#endif

#if RTE_VERSION >= RTE_VERSION_NUM(23, 0, 0, 0)
#define rte_eth_bond_slave_add(a, b)            rte_eth_bond_member_add(a, b)
#define rte_eth_bond_active_slaves_get(a, b, c) rte_eth_bond_active_members_get(a, b, c)
#endif

struct config;
int dpdk_init(struct config *cfg, char *argv0);
void dpdk_run(int (*lcore_main)(void*), void* data);
void dpdk_close(struct config *cfg);

#endif

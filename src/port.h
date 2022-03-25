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

#ifndef __PORT_H
#define __PORT_H

#include <stdbool.h>
#include <stdlib.h>
#include <rte_kni.h>

#include "ip.h"
#include "ip_range.h"
#include "eth.h"
#include "vxlan.h"

#define THREAD_NUM_MAX      64
#define NETIF_PORT_MAX      4
#define PCI_LEN             12

#define NB_RXD              4096
#define NB_TXD              4096

#define RX_BURST_MAX        NB_RXD
#define TX_QUEUE_SIZE       NB_TXD

#define TX_BURST_MAX        1024
#define TX_BURST_DEFAULT    8

#define BOND_SLAVE_MAX      4
#define PCI_NUM_MAX         BOND_SLAVE_MAX
#define BOND_NAME_MAX       32
#define BOND_WAIT_SEC       16

struct netif_port {
    int id; /* DPDK port id */
    int queue_num;
    int socket; /* cpu socket */
    bool enable;
    bool ipv6; /* local_ip, gateway_ip's address family */
    ipaddr_t local_ip;
    ipaddr_t gateway_ip;
    struct eth_addr gateway_mac;
    struct eth_addr local_mac;
    struct rte_mempool *mbuf_pool[THREAD_NUM_MAX];
    struct ip_range *local_ip_range;
    struct ip_range client_ip_range; /* only used by client; server use all client ip range */
    struct ip_range server_ip_range;

    struct rte_kni *kni;
    struct vxlan *vxlan;

    /* bond */
    bool bond;
    uint8_t bond_mode;
    uint8_t bond_policy;
    uint8_t pci_num;
    char bond_name[BOND_NAME_MAX];

    union {
        char pci[PCI_LEN + 1];
        char pci_list[PCI_NUM_MAX][PCI_LEN + 1];
    };

    uint16_t port_id_list[PCI_NUM_MAX];
};

extern uint8_t g_dev_tx_offload_ipv4_cksum;
extern uint8_t g_dev_tx_offload_tcpudp_cksum;

struct config;
int port_init_all(struct config *cfg);
int port_start_all(struct config *cfg);
void port_stop_all(struct config *cfg);
void port_clear(uint16_t port_id, uint16_t queue_id);
int port_config(struct netif_port *port);
struct rte_mempool *port_get_mbuf_pool(struct netif_port *p, int queue_id);

static inline bool port_is_bond4(struct netif_port *port)
{
    return (port->bond && (port->bond_mode == 4));
}

#endif

/*
 * Copyright (c) 2022-2022 Baidu.com, Inc. All Rights Reserved.
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

#include "bond.h"

#include <unistd.h>
#include <stdbool.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>

#include "port.h"
#include "mbuf.h"
#include "net_stats.h"
#include "work_space.h"

int bond_create(struct netif_port *port)
{
    int port_id = 0;

    port_id = rte_eth_bond_create(port->bond_name, port->bond_mode, port->socket);
    if (port_id < 0) {
        printf("create bond error: %s\n", rte_strerror(rte_errno));
        return -1;
    }

    rte_eth_bond_xmit_policy_set(port_id, port->bond_policy);

    return port_id;
}

int bond_config_slaves(struct netif_port *port)
{
    int i = 0;
    uint16_t slave_id = 0;
    struct netif_port port_slave;

    port_slave = *port;
    port_slave.bond = false;
    for (i = 0; i < port->pci_num; i++) {
        slave_id = port->port_id_list[i];
        port_slave.id = slave_id;
        port_config(&port_slave);
        if (rte_eth_bond_slave_add(port->id, slave_id) == -1) {
            printf("add slave %d error\n", slave_id);
            return -1;
        }
    }

    return 0;
}

int bond_wait(struct netif_port *port)
{
    int i = 0;
    int port_id = 0;
    int slave_num = 0;
    uint16_t slaves[BOND_SLAVE_MAX] = {0};

    port_id = port->id;
    slave_num = port->pci_num;

    for (i = 0; i < BOND_WAIT_SEC; i++) {
        if (rte_eth_bond_active_slaves_get(port_id, slaves, BOND_SLAVE_MAX) == slave_num) {
            return 0;
        }
        sleep(1);
    }

    return -1;
}

void bond_broadcast(struct work_space *ws, struct rte_mbuf *m)
{
    int i = 0;
    uint16_t port_id = 0;
    struct rte_mbuf *m2 = NULL;
    struct netif_port *port = NULL;

    port = ws->port;
    for (i = 0; i < port->pci_num; i++) {
        port_id = port->port_id_list[i];
        m2 = mbuf_dup(m);
        if (m2 == NULL) {
            break;
        }

        net_stats_tx(m2);
        if (rte_eth_tx_burst(port_id, ws->queue_id, &m2, 1) != 1) {
            mbuf_free(m2);
        }
    }
}

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

#include "neigh.h"
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include "arp.h"
#include "config.h"
#include "work_space.h"
#include "icmp6.h"
#include "loop.h"
#include "tcp.h"
#include "udp.h"

static void neigh_resolve_gateway_mac_address(struct work_space *ws)
{
    if (ws->queue_id != 0) {
        return;
    }

    /*
     * 1. request gw's mac
     * 2. broadcast local mac
     * */
    if (ws->port->ipv6) {
        icmp6_ns_request(ws);
    } else {
        arp_request_gw(ws);
    }
    work_space_tx_flush(ws);
}

static bool neigh_gateway_is_enable(struct work_space *ws)
{
    struct netif_port *port = NULL;

    port = ws->port;
    if (eth_addr_is_zero(&port->gateway_mac)) {
        return false;
    }

    return true;
}

int neigh_check_gateway(struct work_space *ws)
{
    int i = 0;
    int j = 0;

    for (i = 0; i < NEIGH_SEC; i++) {
        neigh_resolve_gateway_mac_address(ws);
        for (j = 0; j < 1000; j++) {
            if (ws->vxlan) {
                server_recv_mbuf(ws, vxlan_input, tcp_drop, udp_drop);
            } else if (ws->ipv6) {
                server_recv_mbuf(ws, ipv6_input, tcp_drop, udp_drop);
            } else {
                server_recv_mbuf(ws, ipv4_input, tcp_drop, udp_drop);
            }

            work_space_tx_flush(ws);
            if (neigh_gateway_is_enable(ws)) {
                return 0;
            }
            /* sleep 1ms */
            usleep(1000);
        }
    }

    return -1;
}

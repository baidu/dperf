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

#include "lldp.h"
#include "eth.h"
#include "mbuf.h"
#include "bond.h"
#include "work_space.h"

struct lldp_tlv_hdr {
    uint8_t pad:1;
    uint8_t type:7;
    uint8_t len;
};

struct lldp_chassis_id {
    struct lldp_tlv_hdr hdr;
    uint8_t type;
    struct eth_addr mac;
} __attribute__((__packed__));

struct lldp_port_id {
    struct lldp_tlv_hdr hdr;
    uint8_t type;
    struct eth_addr mac;
} __attribute__((__packed__));

struct lldp_ttl {
    struct lldp_tlv_hdr hdr;
    uint16_t ttl;
} __attribute__((__packed__));

struct lldp_end {
    struct lldp_tlv_hdr hdr;
} __attribute__((__packed__));


/* NOTICE: must be 16 bytes */
#define LLDP_STR        "dpdk-dperf-hello"
#define LLDP_STR_SIZE   16

struct lldp_str {
    struct lldp_tlv_hdr hdr;
    char str[LLDP_STR_SIZE];
} __attribute__((__packed__));


struct lldp_packet {
    struct eth_hdr eh;
    struct lldp_chassis_id chassis_id;
    struct lldp_port_id port_id;
    struct lldp_ttl ttl;
    struct lldp_str port_desc;
    struct lldp_str system_name;
    struct lldp_str system_desc;
    struct lldp_end end;
}  __attribute__((__packed__));

static __thread struct lldp_packet g_lldp_packet;

#define LLDP_DMAC   "01:80:c2:00:00:0e"

#define CHASSIS_ID_TLV          1
#define PORT_ID_TLV             2
#define TIME_TO_LIVE_TLV        3
#define PORT_DESCRIPTION_TLV    4
#define SYSTEM_NAME_TLV         5
#define SYSTEM_DESCRIPTION_TLV  6
#define SYSTEM_CAPABILITIES_TLV 7
#define MANAGEMENT_ADDRESS_TLV  8
#define ORG_SPECIFIC_TLV  127
#define END_OF_LLDPDU_TLV 0

/* IEEE 802.3AB Clause 9.5.2: Chassis subtypes */
#define CHASSIS_ID_RESERVED          0
#define CHASSIS_ID_CHASSIS_COMPONENT 1
#define CHASSIS_ID_INTERFACE_ALIAS   2
#define CHASSIS_ID_PORT_COMPONENT    3
#define CHASSIS_ID_MAC_ADDRESS       4
#define CHASSIS_ID_NETWORK_ADDRESS   5
#define CHASSIS_ID_INTERFACE_NAME    6
#define CHASSIS_ID_LOCALLY_ASSIGNED  7
#define CHASSIS_ID_INVALID(t)   (((t) == 0) || ((t) > 7))

/* IEEE 802.3AB Clause 9.5.3: Port subtype */
#define PORT_ID_RESERVED         0
#define PORT_ID_INTERFACE_ALIAS  1
#define PORT_ID_PORT_COMPONENT   2
#define PORT_ID_MAC_ADDRESS      3
#define PORT_ID_NETWORK_ADDRESS  4
#define PORT_ID_INTERFACE_NAME   5
#define PORT_ID_AGENT_CIRCUIT_ID 6
#define PORT_ID_LOCALLY_ASSIGNED 7
#define PORT_ID_INVALID(t)      (((t) == 0) || ((t) > 7))

static void lldp_str_init(struct lldp_str *lldp_str, uint8_t type)
{
    int len = 0;

    len = strlen(LLDP_STR);
    lldp_str->hdr.type = type;
    lldp_str->hdr.len = len;
    memcpy(lldp_str->str, LLDP_STR, len);
}

static void lldp_init_packet(struct work_space *ws)
{
    struct lldp_packet *lldp = NULL;
    struct eth_addr *src = NULL;
    struct netif_port *port = NULL;
    struct eth_addr dst = {.bytes = {0x01, 0x80, 0xc2, 0x00, 0x00, 0x0e}};

    port = ws->port;
    src = &port->local_mac;
    lldp = &g_lldp_packet;

    eth_hdr_set(&lldp->eh, 0x88CC, &dst, src);

    lldp->chassis_id.hdr.type = CHASSIS_ID_TLV;
    lldp->chassis_id.hdr.len = 7;
    lldp->chassis_id.type = 4;
    lldp->chassis_id.mac = *src;

    lldp->port_id.hdr.type = PORT_ID_TLV;
    lldp->port_id.hdr.len = 7;
    lldp->port_id.type = 3;
    lldp->port_id.mac = *src;

    lldp->ttl.hdr.type = TIME_TO_LIVE_TLV;
    lldp->ttl.hdr.len = 2;
    lldp->ttl.ttl = htons(120);

    lldp_str_init(&lldp->port_desc, PORT_DESCRIPTION_TLV);
    lldp_str_init(&lldp->system_name, SYSTEM_NAME_TLV);
    lldp_str_init(&lldp->system_desc, SYSTEM_DESCRIPTION_TLV);

    lldp->end.hdr.type = 0;
    lldp->end.hdr.len = 0;
}

void lldp_init(struct work_space *ws)
{
    struct netif_port *port = ws->port;

    if (port->bond && (port->bond_mode == BONDING_MODE_8023AD)) {
        ws->lldp = true;
        lldp_init_packet(ws);
    }
}

/*
 * In bond mode 4, we need to make the interfaces send some packets every 100ms,
 *  so we send some lldp packets.
 * */
void lldp_send(struct work_space *ws)
{
    int i = 0;
    int num = 0;
    int size = 0;
    struct rte_mbuf *m = NULL;
    struct lldp_packet *lldp = NULL;
    struct netif_port *port = NULL;

    port = ws->port;
    size = sizeof(struct lldp_packet);
    num = port->pci_num * 2;
    for (i = 0; i < num; i++) {
        m = work_space_alloc_mbuf(ws);
        if (m == NULL) {
            return;
        }

        lldp = (struct lldp_packet *)mbuf_push_data(m, size);
        memcpy(lldp, &g_lldp_packet, size);

        work_space_tx_send(ws, m);
    }

    work_space_tx_flush(ws);
}

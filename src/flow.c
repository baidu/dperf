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

/*
 * client
 * ------------------------------------------------
 * each client thread sends requests to one dest address (server)
 * each client thread use one tx/rx queue
 *
 * server
 * ------------------------------------------------
 * each server thread binds one ip address
 * each server thread use one tx/rx queue
 *
 * */

#include "flow.h"

#include <stdint.h>
#include <rte_flow.h>

#include "config.h"

#define MAX_PATTERN_NUM		4

static void flow_pattern_init_eth(struct rte_flow_item *pattern, struct rte_flow_item_eth *spec,
    struct rte_flow_item_eth *mask)
{
    memset(spec, 0, sizeof(struct rte_flow_item_eth));
    memset(mask, 0, sizeof(struct rte_flow_item_eth));
    spec->type = 0;
    mask->type = 0;

    memset(pattern, 0, sizeof(struct rte_flow_item));
    pattern->type = RTE_FLOW_ITEM_TYPE_ETH;
    pattern->spec = spec;
    pattern->mask = mask;
}

static void flow_pattern_init_ipv4(struct rte_flow_item *pattern, struct rte_flow_item_ipv4 *spec,
    struct rte_flow_item_ipv4 *mask, uint32_t sip, uint32_t dip)
{
    uint32_t smask = 0;
    uint32_t dmask = 0;

    if (sip != 0) {
        smask = 0xffffffff;
    }

    if (dip != 0) {
        dmask = 0xffffffff;
    }

    memset(spec, 0, sizeof(struct rte_flow_item_ipv4));
    spec->hdr.dst_addr = dip;
    spec->hdr.src_addr = sip;

    memset(mask, 0, sizeof(struct rte_flow_item_ipv4));
    mask->hdr.dst_addr = dmask;
    mask->hdr.src_addr = smask;

    memset(pattern, 0, sizeof(struct rte_flow_item));
    pattern->type = RTE_FLOW_ITEM_TYPE_IPV4;
    pattern->spec = spec;
    pattern->mask = mask;
}

static void flow_pattern_init_ipv6(struct rte_flow_item *pattern, struct rte_flow_item_ipv6 *spec,
    struct rte_flow_item_ipv6 *mask, ipaddr_t sip, ipaddr_t dip)
{
    uint32_t mask_zero[4] = {0, 0, 0, 0};
    uint32_t mask_full[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    uint32_t *smask = NULL;
    uint32_t *dmask = NULL;

    if (sip.ip != 0) {
        smask = mask_full;
        dmask = mask_zero;
    } else {
        smask = mask_zero;
        dmask = mask_full;
    }

    memset(spec, 0, sizeof(struct rte_flow_item_ipv6));
    memcpy(spec->hdr.dst_addr, &dip, sizeof(struct in6_addr));
    memcpy(spec->hdr.src_addr, &sip, sizeof(struct in6_addr));

    memset(mask, 0, sizeof(struct rte_flow_item_ipv6));
    memcpy(mask->hdr.dst_addr, dmask, sizeof(struct in6_addr));
    memcpy(mask->hdr.src_addr, smask, sizeof(struct in6_addr));

    memset(pattern, 0, sizeof(struct rte_flow_item));
    pattern->type = RTE_FLOW_ITEM_TYPE_IPV6;
    pattern->spec = spec;
    pattern->mask = mask;
}

static void flow_pattern_init_end(struct rte_flow_item *pattern)
{
    memset(pattern, 0, sizeof(struct rte_flow_item));
    pattern->type = RTE_FLOW_ITEM_TYPE_END;
}

static void flow_action_init(struct rte_flow_action *action, struct rte_flow_action_queue *queue, uint16_t rxq)
{
    memset(action, 0, sizeof(struct rte_flow_action) * 2);
    memset(queue, 0, sizeof(struct rte_flow_action_queue));

    queue->index = rxq;
    action[0].type = RTE_FLOW_ACTION_TYPE_QUEUE;
    action[0].conf = queue;
    action[1].type = RTE_FLOW_ACTION_TYPE_END;
}

static int flow_create(uint8_t port_id, struct rte_flow_item *pattern, struct rte_flow_action *action)
{
    int ret = 0;
    struct rte_flow *flow = NULL;
    struct rte_flow_error err;
    struct rte_flow_attr attr;

    memset(&attr, 0, sizeof(struct rte_flow_attr));
    attr.ingress = 1;

    ret = rte_flow_validate(port_id, &attr, pattern, action, &err);
    if (ret < 0) {
        printf("Error: Interface dose not support FDIR. Please use 'rss'!\n");
        return -1;
    }

    flow = rte_flow_create(port_id, &attr, pattern, action, &err);
    if (flow == NULL) {
        printf("Error: Flow create error\n");
        return -1;
    }

    return 0;
}

static int flow_new(uint8_t port_id, uint16_t rxq, ipaddr_t sip, ipaddr_t dip, bool ipv6)
{
    struct rte_flow_action_queue queue;
    struct rte_flow_item_eth eth_spec, eth_mask;
    struct rte_flow_item_ipv4 ip_spec, ip_mask;
    struct rte_flow_item_ipv6 ip6_spec, ip6_mask;
    struct rte_flow_item pattern[MAX_PATTERN_NUM];
    struct rte_flow_action action[MAX_PATTERN_NUM];

    flow_action_init(action, &queue, rxq);
    flow_pattern_init_eth(&pattern[0], &eth_spec, &eth_mask);
    if (ipv6) {
        flow_pattern_init_ipv6(&pattern[1], &ip6_spec, &ip6_mask, sip, dip);
    } else {
        flow_pattern_init_ipv4(&pattern[1], &ip_spec, &ip_mask, sip.ip, dip.ip);
    }
    flow_pattern_init_end(&pattern[2]);

    return flow_create(port_id, pattern, action);
}

static int flow_init_port(struct netif_port *port, bool server, bool ipv6)
{
    int ret = 0;
    int queue_id = 0;
    ipaddr_t server_ip;
    ipaddr_t client_ip;

    memset(&client_ip, 0, sizeof(ipaddr_t));
    rte_flow_flush(port->id, NULL);
    for (queue_id = 0; queue_id < port->queue_num; queue_id++) {
        ip_range_get2(&port->server_ip_range, queue_id, &server_ip);

        if (server) {
            ret = flow_new(port->id, queue_id, client_ip, server_ip, ipv6);
        } else {
            ret = flow_new(port->id, queue_id, server_ip, client_ip, ipv6);
        }

        if (ret < 0) {
            return -1;
        }
    }
    return 0;
}

static int flow_init_vxlan(struct netif_port *port)
{
    int ret = 0;
    int queue_id = 0;
    bool ipv6 = false;  /* vxlan outer headers is ipv4 */
    ipaddr_t local_ip;
    ipaddr_t remote_ip;
    struct vxlan *vxlan = NULL;

    vxlan = port->vxlan;
    memset(&remote_ip, 0, sizeof(ipaddr_t));
    rte_flow_flush(port->id, NULL);
    for (queue_id = 0; queue_id < port->queue_num; queue_id++) {
        ip_range_get2(&vxlan->vtep_local, queue_id, &local_ip);

        ret = flow_new(port->id, queue_id, remote_ip, local_ip, ipv6);
        if (ret < 0) {
            return -1;
        }
    }

    return 0;
}

int flow_init(struct config *cfg)
{
    struct netif_port *port = NULL;
    bool ipv6 = cfg->af == AF_INET6;

    if (cfg->flood) {
        return 0;
    }

    config_for_each_port(cfg, port) {
        if (port->queue_num == 1) {
            continue;
        }
        if (cfg->vxlan) {
            if (flow_init_vxlan(port) < 0) {
                return -1;
            }
        } else {
            if (flow_init_port(port, cfg->server, ipv6) < 0) {
                return -1;
            }
        }
    }

    return 0;
}

void flow_flush(struct config *cfg)
{
    struct netif_port *port = NULL;

    if ((g_config.cpu_num <= 1) || (cfg->flood)) {
        return;
    }

    config_for_each_port(cfg, port) {
        if (port->queue_num == 1) {
            continue;
        }

        rte_flow_flush(port->id, NULL);
    }
}

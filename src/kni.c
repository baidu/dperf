/*
 * Copyright (c) 2022-2022 Baidu.com, Inc. All Rights Reserved.
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

#include "kni.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <rte_ethdev.h>
#include <rte_kni.h>
#include <rte_bus_pci.h>

#include "config.h"
#include "port.h"
#include "mbuf.h"
#include "work_space.h"
#include "bond.h"

static void kni_set_name(struct config *cfg, struct netif_port *port, char *name);

#if RTE_VERSION >= RTE_VERSION_NUM(19,0,0,0)
static int kni_set_mtu(uint16_t port_id, struct rte_kni_conf *conf)
{
    int ret = 0;
    struct rte_eth_dev_info dev_info;

    ret = rte_eth_dev_info_get(port_id, &dev_info);
    if (ret != 0) {
        return -1;
    }

    conf->min_mtu = dev_info.min_mtu;
    conf->max_mtu = dev_info.max_mtu;
	rte_eth_dev_get_mtu(port_id, &conf->mtu);
    return 0;
}
#else
static int kni_set_mtu(__rte_unused uint16_t port_id, __rte_unused struct rte_kni_conf *conf)
{
    return 0;
}
#endif

#if RTE_VERSION >= RTE_VERSION_NUM(18,0,0,0)
/* set mac before rte_kni_alloc */
static int kni_set_mac(struct netif_port *port, struct rte_kni_conf *conf)
{
    memcpy(&(conf->mac_addr), &port->local_mac, ETH_ADDR_LEN);
    return 0;
}

static int kni_set_pci(__rte_unused uint16_t port_id, __rte_unused struct rte_kni_conf *conf)
{
    return 0;
}

static int kni_set_hwaddr(__rte_unused struct config *cfg, __rte_unused struct netif_port *port)
{
    return 0;
}
#else
static int kni_set_mac(__rte_unused struct netif_port *port, __rte_unused struct rte_kni_conf *conf)
{
    return 0;
}

static int kni_set_pci(uint16_t port_id, struct rte_kni_conf *conf)
{
    struct rte_eth_dev_info dev_info;

    memset(&dev_info, 0, sizeof(dev_info));
    rte_eth_dev_info_get(port_id, &dev_info);

    if (dev_info.pci_dev) {
        conf->addr = dev_info.pci_dev->addr;
        conf->id = dev_info.pci_dev->id;
    }

    return 0;
}

/* DPDK-17 need to set MAC by ioctl after rte_kni_alloc */
static int kni_set_hwaddr(struct config *cfg, struct netif_port *port)
{
    int fd = -1;
    int ret = -1;
    struct ifreq ifr;

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    kni_set_name(cfg, port, ifr.ifr_name);

    memcpy(ifr.ifr_hwaddr.sa_data, &port->local_mac, ETH_ADDR_LEN);
    ifr.ifr_hwaddr.sa_family = 1;    /* ARPHRD_ETHER */

    if (ioctl(fd, SIOCSIFHWADDR, &ifr) < 0) {
        printf("Error: kni set hwaddr\n");
    } else {
        ret = 0;
    }

    close(fd);
    return ret;
}
#endif

static void kni_set_name(struct config *cfg, struct netif_port *port, char *name)
{
    int idx = 0;

    /*
     * do not use 'port->id'.
     * we want ifname id starting from zero.
     */
    idx = port - &(cfg->ports[0]);
    snprintf(name, RTE_KNI_NAMESIZE, "%s%d", cfg->kni_ifname, idx);
}

static struct rte_kni *kni_alloc(struct config *cfg, struct netif_port *port)
{
    uint16_t port_id = 0;
    struct rte_kni *kni = NULL;
    struct rte_mempool *mbuf_pool = NULL;
    struct rte_kni_conf conf;

    /* the first thread of a port process this kni */
    mbuf_pool = port->mbuf_pool[0];
    port_id = port->id;
    memset(&conf, 0, sizeof(conf));

    conf.group_id = port_id;
    conf.mbuf_size = RTE_MBUF_DEFAULT_DATAROOM;

    kni_set_name(cfg, port, conf.name);

    if (kni_set_mtu(port_id, &conf) < 0) {
        return NULL;
    }

    if (kni_set_mac(port, &conf) < 0) {
        return NULL;
    }

    if (kni_set_pci(port_id, &conf) < 0) {
        return NULL;
    }

    kni = rte_kni_alloc(mbuf_pool, &conf, NULL);

    if (kni_set_hwaddr(cfg, port) < 0) {
        rte_kni_release(kni);
        return NULL;
    }

    return kni;
}

static int kni_set_link_up(struct config *cfg, struct netif_port *port)
{
    int fd = -1;
    int ret = -1;
    struct ifreq ifr;

    if (!port->kni) {
        return 0;
    }

    fd = socket(PF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        return -1;
    }

    memset(&ifr, 0, sizeof(struct ifreq));
    kni_set_name(cfg, port, ifr.ifr_name);

    if (ioctl(fd, SIOCGIFFLAGS, (void *) &ifr) < 0) {
        printf("kni get flags error\n");
        goto out;
    }
    ifr.ifr_flags |= IFF_UP;
    if (ioctl(fd, SIOCSIFFLAGS, (void *) &ifr) < 0) {
        printf("kni set flags error\n");
        goto out;
    }
    ret = 0;

out:
    close(fd);
    return ret;
}

/*
 * kni_link_up() should be called at the ctl thread.
 *
 * When a kni interface is linked up, the kni will send some messages,
 * which need to be processed by rte_kni_handle_request().
 * These messages have a timeout and need to be processed immediately.
 * Otherwise, the interface will fail to link up.
 */
int kni_link_up(struct config *cfg)
{
    struct netif_port *port = NULL;

    config_for_each_port(cfg, port) {
        if (kni_set_link_up(cfg, port) < 0) {
            return -1;
        }
    }

    return 0;
}

static void kni_free(struct config *cfg)
{
    struct netif_port *port = NULL;

    config_for_each_port(cfg, port) {
        if (port->kni == NULL) {
            continue;
        }

        if (rte_kni_release(port->kni)) {
            printf("failed to free kni\n");
        }
        port->kni = NULL;
        if (!port->kni_mbuf_queue) {
            continue;
        }
        rte_ring_free(port->kni_mbuf_queue);
        port->kni_mbuf_queue = NULL;
    }
}

/*
 * kni address cannot in client or server range
 */
static int kni_create(struct config *cfg)
{
    struct rte_kni *kni = NULL;
    struct netif_port *port = NULL;
    char ring_name[RTE_RING_NAMESIZE];

    rte_kni_init(NETIF_PORT_MAX);
    config_for_each_port(cfg, port) {
        kni = kni_alloc(cfg, port);
        if (kni == NULL) {
            return -1;
        }
        port->kni = kni;
        snprintf(ring_name, sizeof(ring_name), "kr_%d", port->id);
        port->kni_mbuf_queue = rte_ring_create(ring_name, KNI_RING_SIZE, rte_socket_id(), RING_F_SC_DEQ);
        if (!port->kni_mbuf_queue) {
            return -2;
        }
    }

    return 0;
}

int kni_start(struct config *cfg)
{
    if (!cfg->kni) {
        return 0;
    }

    return kni_create(cfg);
}

void kni_stop(struct config *cfg)
{
    if (cfg->kni) {
        kni_free(cfg);
    }
}

void kni_recv(struct work_space *ws, struct rte_mbuf *m)
{
    struct netif_port *port = NULL;
    struct rte_kni *kni = NULL;
    struct rte_mbuf *mbufs[NB_RXD];
    struct rte_ring *kr = NULL;
    int i, cnt, n = 0, send_n=0;

    port = ws->port;
    kni = port->kni;
    kr = port->kni_mbuf_queue;
    /*
     * core that holds queue 0 is in charge of this port's kni work
     * other cores send mbuf to q0 core by kni_ring
     */ 
    if (likely(ws->queue_id != 0)) {
        if (m) {
            /*
             * 1. send to kni_ring 
             * 2. drop packets in other situations
             */
            if (likely(kr && rte_ring_enqueue(kr, (void*)m) == 0)) {
                return;
            }
            mbuf_free2(m); 
        }
        return;
    }
    /* core holds q0 */
    if(m) {
        mbufs[n++] = m;
    }
    if (kr) {
        cnt = RTE_MIN(rte_ring_count(kr), NB_RXD-n);
        if (cnt) {
            n += rte_ring_dequeue_bulk(kr, (void**)&mbufs[n], cnt, NULL);
        }
    }
    if (kni && n) {
        send_n = rte_kni_tx_burst(kni, mbufs, n);
        net_stats_kni_rx(send_n);
    }

    for (i = send_n; i < n; ++i) {
        mbuf_free2(mbufs[i]);
    }
}

void kni_broadcast(struct work_space *ws, struct rte_mbuf *m)
{
    struct rte_mbuf *m2 = NULL;

    m2 = mbuf_dup(m);
    if (m2) {
        kni_recv(ws, m2);
    }
}

static void kni_send_mbuf(struct work_space *ws, struct rte_mbuf *m)
{
    if (port_is_bond4(ws->port) && mbuf_is_neigh(m)) {
        bond_broadcast(ws, m);
    }

    work_space_tx_send(ws, m);
    net_stats_kni_tx(1);
}

void kni_send(struct work_space *ws)
{
    int i = 0;
    int num = 0;
    struct rte_mbuf *mbufs[NB_RXD];
    struct netif_port *port = NULL;
    struct rte_kni *kni = NULL;

    port = ws->port;
    kni = port->kni;
    if(ws->queue_id == 0) {
        rte_kni_handle_request(kni);
        num = rte_kni_rx_burst(kni, mbufs, NB_RXD);
        for (i = 0; i < num; i++) {
            kni_send_mbuf(ws, mbufs[i]);
        }
    }
}

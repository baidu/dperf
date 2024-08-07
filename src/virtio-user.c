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

#include "kni.h"

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/if.h>
#include <rte_ethdev.h>
#include <rte_dev.h>
#include <rte_bus_pci.h>

#include "config.h"
#include "port.h"
#include "mbuf.h"
#include "work_space.h"
#include "bond.h"

#define VDEV_NAME_SIZE 256
#define TX_DESC_PER_QUEUE 512
#define RX_DESC_PER_QUEUE 512
#define VDEV_NAME_FMT "virtio_user%u"
#define VDEV_IFACE_ARGS_FMT "path=/dev/vhost-net,queues=%u,queue_size=%u,iface=%s,mac=%02X:%02X:%02X:%02X:%02X:%02X"
#define VDEV_RING_SIZE 1024
#define QUEUE_NUM 1

#define RTE_ETHER_ADDR_BYTES(mac_addrs)	 ((mac_addrs)->addr_bytes[0]), \
                     ((mac_addrs)->addr_bytes[1]), \
                     ((mac_addrs)->addr_bytes[2]), \
                     ((mac_addrs)->addr_bytes[3]), \
                     ((mac_addrs)->addr_bytes[4]), \
                     ((mac_addrs)->addr_bytes[5])

static void kni_set_name(struct config *cfg, struct netif_port *port, char *name)
{
    int idx = 0;
    /*
     * do not use 'port->id'.
     * we want ifname id starting from zero.
     * */
    idx = port - &(cfg->ports[0]);
    /* 
     * do this because server and client could be on same machine in my env
     * which kni hard to achieve, because in one ns can only open one kni instance
     * */
    if (cfg->server) {
        snprintf(name, VDEV_NAME_SIZE, "%ss%1d", cfg->kni_ifname, idx);
    } else {
        snprintf(name, VDEV_NAME_SIZE, "%sc%1d", cfg->kni_ifname, idx);
    }
}


static inline int
configure_vdev(uint16_t port_id, struct rte_mempool *mb_pool)
{
    int i = 0;
    int ret = 0;
    struct rte_ether_addr addr;
    struct rte_eth_conf default_port_conf = {0};

    if (!rte_eth_dev_is_valid_port(port_id)) {
		return -1;
    }

    ret = rte_eth_dev_configure(port_id, QUEUE_NUM, QUEUE_NUM, &default_port_conf);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "dev config failed\n");
    }

    for (; i < QUEUE_NUM; ++i) {
        ret = rte_eth_tx_queue_setup(port_id, i, TX_DESC_PER_QUEUE,
                rte_eth_dev_socket_id(port_id), NULL);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "queue setup failed\n");
        }
        ret = rte_eth_rx_queue_setup(port_id, i, RX_DESC_PER_QUEUE,
                rte_eth_dev_socket_id(port_id), NULL, mb_pool);
        if (ret < 0) {
            rte_exit(EXIT_FAILURE, "queue setup failed\n");
        }
    }

    ret = rte_eth_dev_start(port_id);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "dev start failed\n");
    }

    ret = rte_eth_macaddr_get(port_id, &addr);
    if (ret != 0) {
        rte_exit(EXIT_FAILURE, "macaddr get failed\n");
    }
    return 0;
}

static uint16_t kni_alloc(struct config *cfg, struct netif_port *port)
{
    uint16_t port_id;
    char vdev_args[VDEV_NAME_SIZE];
    char vdev_name[VDEV_NAME_SIZE];
    char kernel_name[VDEV_NAME_SIZE];

    kni_set_name(cfg, port, kernel_name);

    snprintf(vdev_name, VDEV_NAME_SIZE, VDEV_NAME_FMT, port - &(cfg->ports[0]));

    snprintf(vdev_args, sizeof(vdev_args),
             VDEV_IFACE_ARGS_FMT, QUEUE_NUM,
            VDEV_RING_SIZE, kernel_name, RTE_ETHER_ADDR_BYTES((struct rte_ether_addr*)&port->local_mac));
    if (rte_eal_hotplug_add("vdev", vdev_name,
                vdev_args) < 0) {
        rte_exit(EXIT_FAILURE,
            "vdev creation failed:%s:%d\n",
            __func__, __LINE__);
    }
    if (rte_eth_dev_get_port_by_name(vdev_name, &port_id) != 0) {
        rte_eal_hotplug_remove("vdev", vdev_name);
        rte_exit(EXIT_FAILURE,
            "cannot find added vdev %s:%s:%d\n",
            vdev_name, __func__, __LINE__);
    }
    configure_vdev(port_id, port->mbuf_pool[0]);
    return port_id;
}

static int kni_set_link_up(struct config *cfg, struct netif_port *port)
{
    int fd = -1;
    int ret = -1;
    struct ifreq ifr;

    if (!port->virtio_user_id) {
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
    char vdev_name[VDEV_NAME_SIZE];

    config_for_each_port(cfg, port) {
        if (port->virtio_user_id == 0) {
            continue;
        }
        if (rte_eth_dev_get_name_by_port((uintptr_t)port->virtio_user_id, vdev_name))
            continue;

        rte_eal_hotplug_remove("vdev", vdev_name);
        port->virtio_user_id = 0;

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
    uint16_t port_id = 0;
    struct netif_port *port = NULL;
    char ring_name[RTE_RING_NAMESIZE];

    config_for_each_port(cfg, port) {
        port_id = kni_alloc(cfg, port);
        if (port_id == 0) {
            return -1;
        }
        port->virtio_user_id = port_id;
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
    uint16_t port_id = 0;
    int i = 0;
    int n = 0;
    int cnt = 0;
    int send_n = 0;
    struct rte_ring *kr = NULL;
    struct netif_port *port = NULL;
    struct rte_mbuf *mbufs[NB_RXD];

    port = ws->port;
    port_id = port->virtio_user_id;
    kr = port->kni_mbuf_queue;
    /*
     * core that holds queue 0 is in charge of this port's kni work
     * other cores send mbuf to q0 core by kni_ring
     * */ 
    if (likely(ws->queue_id != 0)) {
        if (m) {
            /*
             * 1. send to kni_ring 
             * 2. drop packets in other situations
             * */
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
    if (port_id && n) {
        send_n = rte_eth_tx_burst(port_id, 0, mbufs, n);
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
    uint16_t kni = 0;
    int i = 0;
    int num = 0;
    struct netif_port *port = NULL;
    struct rte_mbuf *mbufs[NB_RXD];

    port = ws->port;
    kni = port->virtio_user_id;    
    if(ws->queue_id == 0) {
        num = rte_eth_rx_burst(kni, 0, mbufs, NB_RXD);
        for (i = 0; i < num; i++) {
            kni_send_mbuf(ws, mbufs[i]);
        }
    }
}

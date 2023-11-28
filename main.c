/*SPDX-License-Identifier: GPL-3.0-or-later
 * main.c
 *
 *  Created on: 2021-03-02
 *
 */
#include "common.h"
#include "kdns-config.h"
#include "kdns-worker.h"
#include <getopt.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_launch.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION "0.0.1"
#define DEFAULT_CFG_FILE "/etc/kdns/config.cfg"
#define PIDFILE "/var/run/mdns.pid"

static bool DEBUG = false;
static char *kdns_cfgpath = NULL;
static uint16_t nb_rxd = NB_RXD;
static uint16_t nb_txd = NB_TXD;

struct rte_mempool *mdns_pktmbuf_pool = NULL;
struct rte_eth_dev_tx_buffer *tx_buffer[RTE_MAX_ETHPORTS];

struct rte_eth_conf port_conf = {.txmode = {.mq_mode = RTE_ETH_MQ_TX_NONE}};

static void debug_print(const char *fmt, ...)
{
    if (!DEBUG)
    {
        return;
    }
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

static char *parse_progname(char *arg)
{
    char *p;
    if ((p = strrchr(arg, '/')) != NULL)
    {
        return strdup(p + 1);
    }
    return strdup(arg);
}

static int port_init()
{
    uint16_t port_id, nb_port = 0;
    int ret = 0;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_conf eth_conf;
    struct rte_eth_rxconf rx_conf;
    struct rte_eth_txconf tx_conf;
    struct rte_ether_addr addr;

    nb_port = rte_eth_dev_count_avail();
    eth_conf = port_conf;
    unsigned long port_mask = g_kdns_cfg->eal.enabled_nic;
    RTE_ETH_FOREACH_DEV(port_id)
    {
        printf("going to init port %u\n", port_id);
        if ((port_mask & (1 << port_id)) == 0)
        {
            printf("Skipping disabled port %u\n", port_id);
            continue;
        }
        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret != 0)
        {
            printf("Error during getting device (port %u) info: %s\n", port_id,
                   strerror(-ret));
            return -1;
        }
        if (dev_info.tx_offload_capa & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE)
        {
            eth_conf.txmode.offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
        }

        if (g_kdns_cfg->nic.nb_rx_queue > dev_info.max_rx_queues ||
            g_kdns_cfg->nic.nb_tx_queue > dev_info.max_tx_queues)
        {
            printf("Error: queue number is too big, max_rx_queue %d, "
                   "max_tx_queue %d\n",
                   dev_info.max_rx_queues, dev_info.max_tx_queues);
            return -1;
        }

        ret = rte_eth_dev_configure(port_id, g_kdns_cfg->nic.nb_rx_queue,
                                    g_kdns_cfg->nic.nb_tx_queue, &eth_conf);
        if (ret != 0)
        {
            printf("Error during configuring device (port %u): %s\n", port_id,
                   strerror(-ret));
            return -1;
        }

        ret = rte_eth_dev_adjust_nb_rx_tx_desc(port_id, &nb_rxd, &nb_txd);
        if (ret != 0)
        {
            printf("Error during adjusting number of descriptors for device "
                   "(port %u): %s\n",
                   port_id, strerror(-ret));
            return -1;
        }

        rx_conf = dev_info.default_rxconf;
        ret = rte_eth_rx_queue_setup(port_id, 0, nb_rxd,
                                     rte_eth_dev_socket_id(port_id), &rx_conf,
                                     mdns_pktmbuf_pool);
        if (ret < 0)
        {
            printf(
                "Error during setting up rx queue for device (port %u): %s\n",
                port_id, strerror(-ret));
            return -1;
        }

        tx_conf = dev_info.default_txconf;
        tx_conf.offloads |= eth_conf.txmode.offloads;
        ret = rte_eth_tx_queue_setup(port_id, 0, nb_txd,
                                     rte_eth_dev_socket_id(port_id), &tx_conf);
        if (ret < 0)
        {
            printf(
                "Error during setting up tx queue for device (port %u): %s\n",
                port_id, strerror(-ret));
            return -1;
        }

        tx_buffer[port_id] = rte_zmalloc_socket(
            "tx_buffer", RTE_ETH_TX_BUFFER_SIZE(MAX_PKT_BURST), 0,
            rte_eth_dev_socket_id(port_id));
        if (tx_buffer[port_id] == NULL)
        {
            printf(
                "Error during allocating tx buffer for device (port %u): %s\n",
                port_id, strerror(-ret));
            return -1;
        }
        rte_eth_tx_buffer_init(tx_buffer[port_id], MAX_PKT_BURST);

        ret = rte_eth_tx_buffer_set_err_callback(
            tx_buffer[port_id], rte_eth_tx_buffer_count_callback, NULL);
        if (ret < 0)
        {
            printf("Error during setting up tx buffer error callback for "
                   "device (port %u): %s\n",
                   port_id, strerror(-ret));
            return -1;
        }

        ret = rte_eth_macaddr_get(port_id, &addr);
        if (ret)
        {
            printf("Error during getting mac address for device (port %u): "
                   "%s.Set them in config\n",
                   port_id, strerror(-ret));
            // TODO set mac for nic
            //  rte_eth_dev_default_mac_addr_set(port_id,
            //  &g_kdns_cfg->nic.mac_addr);
            return -1;
        }
        printf("Port %u MAC: %02" PRIx8 ":%02" PRIx8 ":%02" PRIx8 ":%02" PRIx8
               ":%02" PRIx8 ":%02" PRIx8 "\n",
               port_id, addr.addr_bytes[0], addr.addr_bytes[1],
               addr.addr_bytes[2], addr.addr_bytes[3], addr.addr_bytes[4],
               addr.addr_bytes[5]);
        ret = rte_eth_dev_start(port_id);
        if (ret < 0)
        {
            printf("Error during starting device (port %u): %s\n", port_id,
                   strerror(-ret));
            return -1;
        }

        ret = rte_eth_promiscuous_enable(port_id);
        if (ret < 0)
        {
            printf("Error during enabling promiscuous mode for device (port "
                   "%u): %s\n",
                   port_id, strerror(-ret));
            return -1;
        }
    }
    return 0;
}

static void parse_args(int argc, char **argv)
{
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1)
    {
        switch (opt)
        {
        case 'c':
            kdns_cfgpath = optarg;
            break;
        default:
            printf("Usage: %s [-c config_file]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (kdns_cfgpath == NULL)
    {
        kdns_cfgpath = DEFAULT_CFG_FILE;
    }

    printf("Using config file: %s\n", kdns_cfgpath);
}

int main(int argc, char **argv)
{
    int ret, i;
    unsigned lcore_id;
    char *dpdk_argv[DPDK_MAX_ARG_NUM];
    uint16_t nb_port = 0;

    DEBUG = getenv("DEBUG") ? 1 : 0;
    parse_args(argc, argv);

    ret = kdns_load_conf(kdns_cfgpath, parse_progname(argv[0]));
    if (ret < 0)
        exit(EXIT_FAILURE);

    for (i = 0; i < g_kdns_cfg->eal.argc; i++)
    {
        dpdk_argv[i] = strdup(g_kdns_cfg->eal.argv[i]);
        printf("%s\n", dpdk_argv[i]);
    }
    ret = rte_eal_init(g_kdns_cfg->eal.argc, dpdk_argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    nb_port = rte_eth_dev_count_avail();
    mdns_pktmbuf_pool = rte_pktmbuf_pool_create(
        "mdns_pktmbuf_pool", NB_MBUF * nb_port, MEMPOOL_CACHE_SIZE, 0,
        RTE_MBUF_DEFAULT_BUF_SIZE, rte_socket_id());
    if (mdns_pktmbuf_pool == NULL)
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    ret = port_init();
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "port_init failed\n");

    rte_eal_mp_remote_launch(dns_worker_start, NULL, SKIP_MAIN);
    RTE_LCORE_FOREACH_WORKER(lcore_id)
    {
        rte_eal_wait_lcore(lcore_id);
    }
    rte_eal_cleanup();
    return 0;
}

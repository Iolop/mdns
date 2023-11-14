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
#include <rte_launch.h>
#include <rte_lcore.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define VERSION "0.0.1"
#define DEFAULT_CFG_FILE "/etc/kdns/config.cfg"
#define PIDFILE "/var/run/mdns.pid"

static bool DEBUG = false;
static char *kdns_cfgpath = NULL;

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

    nb_port = rte_eth_dev_count_avail();
    eth_conf = port_conf;
    RTE_ETH_FOREACH_DEV(port_id)
    {
        ret = rte_eth_dev_info_get(port_id, &dev_info);
        if (ret != 0)
        {
            printf("Error during getting device (port %u) info: %s\n", port_id, strerror(-ret));
            return -1;
        }
        if (g_kdns_cfg->nic.nb_rx_queue > dev_info.max_rx_queues ||
            g_kdns_cfg->nic.nb_tx_queue > dev_info.max_tx_queues)
        {
            printf("Error: queue number is too big, max_rx_queue %d, max_tx_queue %d\n",
                   dev_info.max_rx_queues, dev_info.max_tx_queues);
            return -1;
        }
        ret = rte_eth_dev_configure(port_id, g_kdns_cfg->nic.nb_rx_queue,
                                    g_kdns_cfg->nic.nb_tx_queue, &eth_conf);
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

    DEBUG = getenv("DEBUG") ? 1 : 0;
    parse_args(argc, argv);

    ret = kdns_load_conf(kdns_cfgpath, parse_progname(argv[0]));
    if (ret < 0)
        exit(EXIT_FAILURE);

    for (i = 0; i < g_kdns_cfg->eal.argc; i++)
    {
        dpdk_argv[i] = strdup(g_kdns_cfg->eal.argv[i]);
    }
    ret = rte_eal_init(g_kdns_cfg->eal.argc, dpdk_argv);
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

    ret = port_init();
    if (ret < 0)
        rte_exit(EXIT_FAILURE, "port_init failed\n");

    RTE_LCORE_FOREACH_WORKER(lcore_id)
    {
        rte_eal_remote_launch(dns_worker_start, NULL, lcore_id);
    }
    RTE_LCORE_FOREACH_WORKER(lcore_id)
    {
        rte_eal_wait_lcore(lcore_id);
    }
    return 0;
}

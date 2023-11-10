/*SPDX-License-Identifier: GPL-3.0-or-later
 * main.c
 *
 *  Created on: 2021-03-02
 * 
*/
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_common.h>
#include "kdns-config.h"

#define VERSION             "0.0.1"
#define DEFAULT_CFG_FILE    "/etc/kdns/config.cfg"
#define PIDFILE             "/var/run/mdns.pid"

static bool DEBUG = false;
static char *kdns_cfgpath = NULL;

static void debug_print(const char *fmt, ...)
{
    if (!DEBUG) {
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

static void parse_args(int argc, char **argv) {
    int opt;

    while ((opt = getopt(argc, argv, "c:")) != -1) {
        switch (opt) {
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

int main(int argc, char **argv) {
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
    
    return 0;
}

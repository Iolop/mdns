#include "kdns-config.h"
#include <rte_cfgfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct kdns_conf *g_kdns_cfg = NULL;

static int kdns_eal_init(struct rte_cfgfile *cfg, struct eal_config *eal, char *procname)
{
    int ret = 0;
    const char *entry;

    strncpy(eal->argv[eal->argc++], procname, strlen(procname));

    entry = rte_cfgfile_get_entry(cfg, "EAL", "cores");
    if (entry)
    {
        snprintf(eal->argv[eal->argc++], DPDK_MAX_ARG_LEN, "-l%s", entry);
    }
    else
    {
        printf("Failed to get cores from config file\n");
        return -1;
    }

    entry = rte_cfgfile_get_entry(cfg, "EAL", "mem_channels");
    if (entry)
    {
        snprintf(eal->argv[eal->argc++], DPDK_MAX_ARG_LEN, "-n%s", entry);
    }
    else
    {
        printf("Failed to get mem_channels from config file\n");
        return -1;
    }
    return 0;
}

static int kdns_nic_init(struct rte_cfgfile *cfg, struct nic_config *nic)
{
    int ret = 0;
    const char *entry;

    entry = rte_cfgfile_get_entry(cfg, "NIC", "nb_rx_queue");
    if (entry)
    {
        nic->nb_rx_queue = atoi(entry);
    }
    else
    {
        printf("Failed to get nb_rx_queue from config file\n");
        return -1;
    }

    entry = rte_cfgfile_get_entry(cfg, "NIC", "nb_tx_queue");
    if (entry)
    {
        nic->nb_tx_queue = atoi(entry);
    }
    else
    {
        printf("Failed to get nb_tx_queue from config file\n");
        return -1;
    }
    return 0;
}

int kdns_load_conf(char *cfgpath, char *procname)
{
    int ret = 0;

    g_kdns_cfg = calloc(1, sizeof(struct kdns_conf));
    if (!g_kdns_cfg)
    {
        printf("Failed to allocate memory for kdns config\n");
        return -1;
    }
    struct rte_cfgfile *cfg = rte_cfgfile_load(cfgpath, 0);
    if (!cfg)
    {
        printf("Failed to load config file: %s\n", cfgpath);
        return -1;
    }
    ret |= kdns_eal_init(cfg, &g_kdns_cfg->eal, procname);
    ret |= kdns_nic_init(cfg, &g_kdns_cfg->nic);

    return ret;
}
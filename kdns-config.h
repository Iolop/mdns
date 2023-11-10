#ifndef __KDNS_CONF_H__
#define __KDNS_CONF_H__

#define DPDK_MAX_ARG_NUM (32)
#define DPDK_MAX_ARG_LEN (1024)

struct eal_config
{
    int argc;
    char argv[DPDK_MAX_ARG_NUM][DPDK_MAX_ARG_LEN];
};

struct kdns_conf
{
    struct eal_config eal;
};

extern struct kdns_conf *g_kdns_cfg;

int kdns_load_conf(char *cfgpath, char *procname);

#endif
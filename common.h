#ifndef __COMMON_H__
#define __COMMON_H__
#include <rte_ethdev.h>

#define NB_RXD (1024)
#define NB_TXD (1024)
#define NB_MBUF (2048)
#define MEMPOOL_CACHE_SIZE (64)
#define MAX_PKT_BURST (32)
struct rte_eth_conf port_conf = {.rxmode = {.mq_mode = RTE_ETH_MQ_RX_RSS},
                                 .txmode = {.mq_mode = RTE_ETH_MQ_TX_NONE}};
#endif
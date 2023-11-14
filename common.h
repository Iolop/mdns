#ifndef __COMMON_H__
#define __COMMON_H__
#include <rte_ethdev.h>
struct rte_eth_conf port_conf = {.rxmode = {.mq_mode = RTE_ETH_MQ_RX_RSS}, .txmode = {.mq_mode = RTE_ETH_MQ_TX_NONE}};
#endif
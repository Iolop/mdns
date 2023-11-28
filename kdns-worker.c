#include "common.h"
#include "kdns-config.h"
#include <netinet/in.h>
#include <rte_ethdev.h>
#include <rte_ether.h>
#include <rte_lcore.h>
#include <rte_mbuf_core.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned int *ports_matched;

static int packet_process_start(struct rte_mbuf **pkts_burst,
                                unsigned int nb_rx)
{
    struct rte_mbuf *m;
    struct rte_ether_hdr *ether_h;
    unsigned int i;
    for (i = 0; i < nb_rx; i++)
    {
        m = pkts_burst[i];
        ether_h = rte_pktmbuf_mtod(m, struct rte_ether_hdr *);
        if (ether_h->ether_type == rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4))
        {
            struct rte_ipv4_hdr *ipv4_h;
            ipv4_h = (struct rte_ipv4_hdr *)((unsigned char *)ether_h +
                                             sizeof(struct rte_ether_hdr));
            if (ipv4_h->next_proto_id == IPPROTO_ICMP)
            {
                struct rte_icmp_hdr *icmp_h;
                icmp_h = (struct rte_icmp_hdr *)((unsigned char *)ipv4_h +
                                                 sizeof(struct rte_ipv4_hdr));
                if (icmp_h->icmp_type == RTE_IP_ICMP_ECHO_REQUEST)
                {
                    printf("icmp echo request\n");
                    icmp_h->icmp_type = RTE_IP_ICMP_ECHO_REPLY;
                    icmp_h->icmp_cksum = 0;
                    icmp_h->icmp_cksum =
                        rte_raw_cksum(icmp_h, sizeof(struct rte_icmp_hdr));
                    ipv4_h->hdr_checksum = 0;
                    ipv4_h->hdr_checksum = rte_ipv4_cksum(ipv4_h);
                    struct rte_ether_addr tmp;
                    rte_ether_addr_copy(&ether_h->src_addr, &tmp);
                    rte_ether_addr_copy(&ether_h->dst_addr, &ether_h->src_addr);
                    rte_ether_addr_copy(&tmp, &ether_h->dst_addr);
                    rte_eth_tx_burst(0, 0, &m, 1);
                }
                else
                {
                    printf("icmp type %d\n", icmp_h->icmp_type);
                }
            }
            else
            {
                printf("ipv4_h->next_proto_id = %d\n", ipv4_h->next_proto_id);
            }
        }
        else
        {
            printf("not ipv4 packet\n");
        }
    }
    return 0;
}

static inline unsigned int *get_match_ports(unsigned int lcore_id)
{
    ports_matched = calloc(2, sizeof(unsigned int));
    ports_matched[0] = lcore_id - 1;

    printf("lcore %d match ports %d\n", lcore_id, lcore_id - 1);
    // ports_matched[1] = 1;
    return ports_matched;
}

// TODO: maybe i should set ports which enabled in this lcore with a list in arg
// parameter as for now ,only 1 queue per port is enabled. port1 matches lcore1,
// and so far.
int dns_worker_start(void *arg)
{
    struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
    unsigned int lcore_id = rte_lcore_id();
    if (lcore_id == 2)
        return 0;
    printf("kdns_worker load from lcore %d\n", lcore_id);
    get_match_ports(lcore_id);
    while (1)
    {
        unsigned int nb_rx = 0;
        unsigned int i;
        for (i = 0; i < 1; i++)
        {
            nb_rx = rte_eth_rx_burst(ports_matched[i], 0, pkts_burst,
                                     MAX_PKT_BURST);
            if (nb_rx)
            {
                packet_process_start(pkts_burst, nb_rx);
                for (int j = 0; j < nb_rx; j++)
                {
                    rte_pktmbuf_free(pkts_burst[j]);
                }
            }
        }
    }
    return 0;
}
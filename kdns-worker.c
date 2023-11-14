#include <rte_lcore.h>
#include <stdio.h>
#include <stdlib.h>

int dns_worker_start(void *arg) {
  printf("kdns_worker load from lcore %d\n", rte_lcore_id());
  return 0;
}
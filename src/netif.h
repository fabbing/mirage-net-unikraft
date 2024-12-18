#ifndef NETIF_H
#define NETIF_H

#include <uk/netdev.h>
#include <uk/assert.h>
#include <uk/print.h>

//#define MNU_DEBUG 1

#ifdef MNU_DEBUG
#define UK_DEBUG 1
#endif /* !MNU_DEBUG */

struct netif {
  struct uk_netdev      *dev;
  struct uk_alloc       *alloc;
  struct uk_netdev_info dev_info;
  unsigned              id;
};

/* netbuf.c */
struct uk_netbuf *netdev_alloc_rx_netbuf(const struct netif *netif);
struct uk_netbuf *netdev_alloc_tx_netbuf(const struct netif *netif);

/* rx.c */
uint16_t netdev_alloc_rxpkts(void *argp, struct uk_netbuf *nb[],
    uint16_t count);

#endif /* !NETIF_H */

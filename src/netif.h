/*
 * The MIT License
 *
 * Copyright (c) 2024, Tarides
 * Author(s): Fabrice Buoro <fabrice@tarides.com>
 * Inspired by mirage-net-solo5 and lib-lwip
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef NETIF_H
#define NETIF_H

#include <uk/netdev.h>
#include <uk/assert.h>
#include <uk/print.h>

#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>

#ifdef MNU_DEBUG
#define UK_DEBUG 1
#endif /* !MNU_DEBUG */


struct netif {
  struct uk_netdev      *dev;
  struct uk_alloc       *alloc;
  struct uk_netdev_info dev_info;
  unsigned              id;
};

/* netif_.c */
CAMLprim value alloc_result_ok(void);
CAMLprim value alloc_result_error(const char *msg);

/* netbuf.c */
struct uk_netbuf *netdev_alloc_rx_netbuf(const struct netif *netif);
struct uk_netbuf *netdev_alloc_tx_netbuf(const struct netif *netif);

/* rx.c */
uint16_t netdev_alloc_rxpkts(void *argp, struct uk_netbuf *nb[],
        uint16_t count);

#endif /* !NETIF_H */

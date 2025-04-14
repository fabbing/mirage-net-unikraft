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

#include "netif.h"

// Size of the buffer area for uk_netbuf allocation
#define UKNETDEV_BUFLEN   2048


struct uk_netbuf *netdev_alloc_rx_netbuf(const struct netif *netif)
{
  struct uk_netbuf *netbuf = NULL;
  struct uk_alloc *alloc = netif->alloc;
  const size_t size = UKNETDEV_BUFLEN;
  const size_t align = netif->dev_info.ioalign;
  const size_t headroom = netif->dev_info.nb_encap_rx;
  const size_t priv_len = 0;

  netbuf = uk_netbuf_alloc_buf(alloc, size, align, headroom, priv_len, NULL);
  if (!netbuf) {
    return NULL;
  }
  netbuf->len = netbuf->buflen - headroom;
  return netbuf;
}

struct uk_netbuf *netdev_alloc_tx_netbuf(const struct netif *netif)
{
  struct uk_netbuf *netbuf = NULL;
  struct uk_alloc *alloc = netif->alloc;
  const size_t size = UKNETDEV_BUFLEN;
  const size_t align = netif->dev_info.ioalign;
  const size_t headroom = netif->dev_info.nb_encap_tx;
  const size_t priv_len = 0;

  netbuf = uk_netbuf_alloc_buf(alloc, size, align, headroom, priv_len, NULL);
  if (!netbuf) {
    return NULL;
  }
  return netbuf;
}

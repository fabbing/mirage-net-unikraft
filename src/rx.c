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

#include <string.h>
#include <caml/bigarray.h>

#include <yield.h> /* provided by mirage-unikraft */

uint16_t netdev_alloc_rxpkts(void *argp, struct uk_netbuf *nb[], uint16_t count)
{
  struct netif *netif = (struct netif*)argp;
  uint16_t i;

  for (i = 0; i < count; ++i) {
    nb[i] = netdev_alloc_rx_netbuf(netif);
    if (!nb[i]) {
      /* we run out of memory */
      break;
    }
  }
  return i;
}

/* Returns
* -1 on error
*  0 on success
*  1 on success and more to process
*/
static int netdev_rx(struct netif* netif, uint8_t *buf, unsigned *size,
        const char **err)
{
  struct uk_netdev *dev = netif->dev;
  struct uk_netbuf *nb;
  unsigned bufsize = *size;

  *size = 0;

  const int rc = uk_netdev_rx_one(dev, 0, &nb);
  if (rc < 0) {
    *err = "Failed to receive a packet";
    return -1;
  }

  if (!uk_netdev_status_successful(rc)) {
    *err = "No packet received";
    return 0;
  }

  const bool more = uk_netdev_status_more(rc);

  if (bufsize <= nb->len) {
    *err = "Not enough room in buffer to write packet";
    uk_netbuf_free_single(nb);
    return -1;
  }

  memcpy(buf, nb->data, nb->len);
  *size = (nb->len > 10) ? nb->len - 10 : nb->len; // FIXME
  uk_netbuf_free_single(nb);

  return (more ? 1 : 0);
}

CAMLprim value uk_netdev_rx(value v_netif, value v_buf, value v_size)
{
  CAMLparam3(v_netif, v_buf, v_size);
  CAMLlocal1(v_result);

  struct netif *netif = (struct netif*)Ptr_val(v_netif);
  uint8_t *buf = (uint8_t *)Caml_ba_data_val(v_buf);
  unsigned size = Int_val(v_size);
  const char *err = NULL;

  const int rc = netdev_rx(netif, buf, &size, &err);
  if (rc < 0) {
    v_result = alloc_result_error(err);
    CAMLreturn(v_result);
  }

  if (rc > 0) {
    set_netdev_queue_ready(netif->id);
  }
  else {
    set_netdev_queue_empty(netif->id);
  }

  v_result = alloc_result_ok();
  Store_field(v_result, 0, Val_int(size));
  CAMLreturn(v_result);
}

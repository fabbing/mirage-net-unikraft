#include "netif.h"
#include "misc.h"

#include <string.h>
#include <stdio.h> // XXX

static int netdev_tx(struct uk_netdev *netdev, struct uk_netbuf *netbuf,
        int64_t size, const char **err)
{
  int rc;

  netbuf->len = size;

#ifdef MNU_DEBUG
  printf("---- TX ----\n");
  debug_bytes(netbuf->data, netbuf->len);
  printf("------------\n");
#endif

  do {
    rc = uk_netdev_tx_one(netdev, 0, netbuf);
  } while (uk_netdev_status_notready(rc));

  if (rc < 0) {
    *err = "Failed to tx_one";
    uk_netbuf_free_single(netbuf);
    return -1;
  }

  uk_pr_debug("Wrote a packet of %ld bytes");
  return 0;
}

static struct uk_netbuf* get_tx_buffer(struct netif *netif, size_t size,
        const char **err)
{
  struct uk_netbuf *nb;

  nb = netdev_alloc_tx_netbuf(netif);
  if (!nb) {
    *err = "Failed to allocate netbuf";
    return NULL;
  }

  const size_t tailroom = uk_netbuf_tailroom(nb);
  if (size  > tailroom) {
    *err = "Not enough tailroom for data";
    uk_netbuf_free_single(nb);
    return NULL;
  }
  nb->len = size;
  memset(nb->data, 0, nb->len);
  return nb;
}


#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>
#include <caml/bigarray.h>

CAMLprim value uk_get_tx_buffer(value v_netif, value v_size)
{
  CAMLparam2(v_netif, v_size);
  CAMLlocal1(v_result);
  struct netif *netif = (struct netif*)Ptr_val(v_netif);
  size_t size = Int_val(v_size);
  const char *err = NULL;

  struct uk_netbuf *nb = get_tx_buffer(netif, size, &err);
  if (!nb) {
    v_result = alloc_result_error(err);
    CAMLreturn(v_result);
  }

  v_result = alloc_result_ok();
  Store_field(v_result, 0, Val_ptr(nb));
  CAMLreturn(v_result);
}

CAMLprim value uk_bigarray_of_netbuf(value v_netbuf)
{
  CAMLparam1(v_netbuf);
  CAMLlocal1(v_ba);

  struct uk_netbuf *nb = (struct uk_netbuf*)Ptr_val(v_netbuf);

  v_ba = caml_ba_alloc_dims(CAML_BA_CHAR | CAML_BA_C_LAYOUT, 1, nb->data,
      nb->len);

  CAMLreturn(v_ba);
}

CAMLprim value uk_netdev_tx(value v_netif, value v_netbuf, value v_size)
{
  CAMLparam3(v_netif, v_netbuf, v_size);
  CAMLlocal1(v_result);
  struct netif *netif = (struct netif*)Ptr_val(v_netif);
  struct uk_netbuf *netbuf = (struct uk_netbuf*)Ptr_val(v_netbuf);
  int64_t size = Int_val(v_size);
  const char *err = NULL;

  const int ret = netdev_tx(netif->dev, netbuf, size, &err);
  if (ret) {
      v_result = alloc_result_error(err);
    CAMLreturn(v_result);
  }

  v_result = alloc_result_ok();
  Store_field(v_result, 0, Val_unit);
  CAMLreturn(v_result);
}

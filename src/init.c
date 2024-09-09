#include "netif.h"

#include "yield.h"

#include <stdlib.h>
#include <string.h>

static struct netif* init_netif(unsigned id)
{
  struct netif *netif = malloc(sizeof(*netif));
  memset(netif, 0, sizeof(*netif));

  netif->alloc = uk_alloc_get_default();
  if (!netif->alloc) {
    uk_pr_err("Failed to get default allocator");
    free(netif);
    return NULL;
  }
  netif->id = id;
  return netif;
}

static void netdev_queue_event(struct uk_netdev *netdev, uint16_t queueid,
    void *argp)
{
  struct netif *netif = argp;

  (void)netdev;
  (void)queueid;
  signal_netdev_queue_ready(netif->id);
}

static int netdev_configure(struct netif *netif, const char **err)
{
  struct uk_netdev *dev = netif->dev;
  struct uk_netdev_info *dev_info = &netif->dev_info;
  int rc;

  UK_ASSERT(dev != NULL);
  UK_ASSERT(uk_netdev_state_get(dev) == UK_NETDEV_UNCONFIGURED);

  uk_netdev_info_get(dev, dev_info);
  if (!dev_info->max_rx_queues || !dev_info->max_tx_queues) {
    *err = "Invalid device information";
    return -1;
  }

  struct uk_netdev_conf dev_conf = {0};
  dev_conf.nb_rx_queues = 1;
  dev_conf.nb_tx_queues = 1;
  rc = uk_netdev_configure(dev, &dev_conf);
  if (rc < 0) {
    *err = "Error configuring device";
    return -1;
  }

  struct uk_netdev_rxqueue_conf rxq_conf = {0};
  rxq_conf.a = netif->alloc;
  rxq_conf.alloc_rxpkts = netdev_alloc_rxpkts;
  rxq_conf.alloc_rxpkts_argp = netif;

  rxq_conf.callback = netdev_queue_event;
  rxq_conf.callback_cookie = netif;
  rxq_conf.s = uk_sched_current();
  if (!rxq_conf.s) {
    *err = "Unable to get current scheduler";
    return -1;
  }

  rc = uk_netdev_rxq_configure(dev, 0, 0, &rxq_conf);
  if (rc < 0) {
    *err = "Error configuring RX queue";
    return -1;
  }

  struct uk_netdev_txqueue_conf txq_conf = {0};
  txq_conf.a = netif->alloc;
  rc = uk_netdev_txq_configure(dev, 0, 0, &txq_conf);
  if (rc < 0) {
    *err = "Error configuring TX queue";
    return -1;
  }

  rc = uk_netdev_start(dev);
  if (rc < 0) {
    *err = "Error starting netdev";
    return -1;
  }

  if (!uk_netdev_rxintr_supported(dev_info->features)) {
    *err = "Device doesn't support RX interrupt";
    return -1;
  }
  rc = uk_netdev_rxq_intr_enable(dev, 0);
  if (rc < 0) {
    *err = "Failed to enable RX interrupts";
    return -1;
  }
  else if (rc == 1) {
    // TODO drain queue
  }

  return 0;
}

static struct netif* netdev_get(unsigned int id, const char **err)
{
  struct netif *netif;
  // FIXME need a semaphore? (uk_semaphore_init)

  netif = init_netif(id);
  if (!netif) {
    *err = "Failed to allocate memory for netif";
    return NULL;
  }

  struct uk_netdev *netdev = uk_netdev_get(id);
  if (!netdev) {
    *err = "Failed to acquire network device";
    return NULL;
  }
  netif->dev = netdev;

  if (uk_netdev_state_get(netdev) != UK_NETDEV_UNPROBED) {
    *err = "Network device not in unprobed state";
    return NULL;
  }
  const int rc = uk_netdev_probe(netdev);
  if (rc < 0) {
    *err = "Failed to probe network device";
    return NULL;
  }

  if (uk_netdev_state_get(netdev) != UK_NETDEV_UNCONFIGURED) {
    *err = "Network device not in unconfigured state";
    return NULL;
  }

  return netif;
}

#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>

#include <assert.h> // TODO remove

// FIXME return a Result?
CAMLprim value uk_netdev_init(value v_id)
{
  CAMLparam1(v_id);
  CAMLlocal2(v_result, v_error);

  v_result = caml_alloc_tuple(3);
  Store_field(v_result, 0, Val_false);
  Store_field(v_result, 1, caml_copy_int64(0));
  Store_field(v_result, 2, caml_copy_string(""));

  const char *err = NULL;
  assert(Int_val(v_id) >= 0); // TODO do it on OCaml side
  const unsigned id = Int_val(v_id);
  struct netif *netif = netdev_get(id, &err);
  if (!netif) {
    v_error = caml_copy_string(err);
    Store_field(v_result, 2, v_error);
    CAMLreturn(v_result);
  }

  const int rc = netdev_configure(netif, &err);
  if (rc) {
    v_error = caml_copy_string(err);
    Store_field(v_result, 2, v_error);
    CAMLreturn(v_result);
  }

  Store_field(v_result, 0, Val_true);
  Store_field(v_result, 1, caml_copy_int64((intptr_t)netif));
  CAMLreturn(v_result);
}


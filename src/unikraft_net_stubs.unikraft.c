// TODO header and acknowledge lib_lwip

#include "netif.h"

#include <string.h>

#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h>


CAMLprim value uk_netdev_mac(value v_netif)
{
    CAMLparam1(v_netif);
    CAMLlocal1(v_mac);

    const struct netif *netif = (struct netif*)Int64_val(v_netif);
    const struct uk_hwaddr *hwaddr = uk_netdev_hwaddr_get(netif->dev);

    const size_t len = 6;
    v_mac = caml_alloc_string(len);
    memcpy(Bytes_val(v_mac), hwaddr->addr_bytes, len);
    CAMLreturn(v_mac);
}

CAMLprim value uk_netdev_mtu(value v_netif)
{
    CAMLparam1(v_netif);

    const struct netif *netif = (struct netif*)Int64_val(v_netif);
    const uint16_t mtu = uk_netdev_mtu_get(netif->dev);

    fprintf(stderr, "uk_netdev_mtu = %d\n", mtu); // XXX
    CAMLreturn(Val_int((int)mtu));
}

#include "misc.h"

#include <ctype.h>
#include <stdio.h>

void debug_bytes(const unsigned char *data, size_t len)
{
  for (size_t i = 0; i < len; i += 16) {
    printf("%08lx  ", i);
    for (size_t j = 0; j < 16; ++j) {
      if (i + j < len)
        printf("%02x ", data[i+j]);
      else
        printf("   ");
    }
    printf("|");
    for (size_t j = 0; j < 16 && i + j < len; ++j) {
      char c = data[i+j];
      if (isprint(c))
        printf("%c", c);
      else
        printf(".");
    }
    printf("|\n");
  }
}

#define CAML_NAME_SPACE
#include <caml/memory.h>
#include <caml/alloc.h>

CAMLprim value alloc_result_ok(void)
{
  CAMLparam0();
  CAMLlocal1(v_result);

  v_result = caml_alloc(1, 0);
  CAMLreturn(v_result);
}

CAMLprim value alloc_result_error(const char *msg)
{
  CAMLparam0();
  CAMLlocal2(v_result, v_error);

  v_result = caml_alloc(1, 1);
  v_error = caml_copy_string(msg);
  Store_field(v_result, 0, v_error);
  CAMLreturn(v_result);
};

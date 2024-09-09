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

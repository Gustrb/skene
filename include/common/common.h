#ifndef SKENE_COMMON_H
#define SKENE_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define UNUSED(x) ((void)(x))

#define TODO(msg) \
  fprintf(stderr, msg "\n"); \
  exit(9999);

#define PUBLIC
#define PRIVATE static

#endif

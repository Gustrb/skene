#ifndef SKENE_COMMON_H
#define SKENE_COMMON_H

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#define UNUSED(x) ((void)(x))

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

#define TODO(msg) \
  fprintf(stderr, msg "\n"); \
  exit(9999);

#define PUBLIC
#define PRIVATE static

#define PLATFORM_SPECIFIC(p) p

#define ASSUME(p) do { if (!(p)) PLATFORM_SPECIFIC(__builtin_unreachable()); } while(0);

#define TRY(c) do { int32_t err = c; if (err != 0) return err; } while (0);

#endif

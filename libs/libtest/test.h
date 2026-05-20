#ifndef SKENE_LIBTEST_H
#define SKENE_LIBTEST_H

#if !defined(__linux__) || !defined(__x86_64__)
#error "skene libtest supports only x86_64 on linux"
#endif

#include <common/common.h>

#define SKENE_LIBTEST_TAP_VERSION "13"

static int __skene_test_count = 0;
static int __skene_tests_failed = 0;
static int __skene_planned_count = 0;

#define TEST_START(plan)                                                       \
  do {                                                                         \
    __skene_planned_count = plan;                                              \
    fprintf(stdout, "TAP version " SKENE_LIBTEST_TAP_VERSION "\n");             \
    fprintf(stdout, "1..%d\n", __skene_planned_count);                         \
  } while (0)

#define ASSERT_BOOL(cond, desc)                                                \
  do {                                                                         \
    __skene_test_count++;                                                      \
    if (cond) {                                                                \
      fprintf(stdout, "ok %d - %s\n", __skene_test_count, desc);               \
    } else {                                                                   \
      __skene_tests_failed++;                                                  \
      fprintf(stdout, "not ok %d - %s\n", __skene_test_count, desc);           \
      fprintf(stdout, "  ---\n");                                              \
      fprintf(stdout, "  message: 'Assertion Failed'\n");                      \
      fprintf(stdout, "  file: '%s'\n", __FILE__);                             \
      fprintf(stdout, "  line: '%d'\n", __LINE__);                             \
      fprintf(stdout, "  ...\n");                                              \
    }                                                                          \
  } while (0)

#define ASSERT_INT_EQ(expected, actual, desc)                                  \
  do {                                                                         \
    __skene_test_count++;                                                       \
    if ((expected) == (actual)) {                                              \
      fprintf(stdout, "ok %d - %s\n", __skene_test_count, desc);                \
    } else {                                                                   \
      fprintf(stdout, "not ok %d - %s\n", __skene_test_count, desc);            \
      fprintf(stdout, "  ---\n");                                              \
      fprintf(stdout, "  message: 'Integer mismatch'\n");                      \
      fprintf(stdout, "  expected: %d\n", (int)(expected));                    \
      fprintf(stdout, "  actual:   %d\n", (int)(actual));                      \
      fprintf(stdout, "  file: '%s'\n", __FILE__);                             \
      fprintf(stdout, "  line: %d\n", __LINE__);                               \
      fprintf(stdout, "  ...\n");                                              \
    }                                                                          \
  } while (0)

#define ASSERT_CHAR_EQ(expected, actual, desc)                                  \
  do {                                                                         \
    __skene_test_count++;                                                      \
    if ((expected) == (actual)) {                                              \
      fprintf(stdout, "ok %d - %s\n", __skene_test_count, desc);               \
    } else {                                                                   \
      fprintf(stdout, "not ok %d - %s\n", __skene_test_count, desc);           \
      fprintf(stdout, "  ---\n");                                              \
      fprintf(stdout, "  message: 'Char mismatch'\n");                      \
      fprintf(stdout, "  expected: %c\n", (int)(expected));                    \
      fprintf(stdout, "  actual:   %c\n", (int)(actual));                      \
      fprintf(stdout, "  file: '%s'\n", __FILE__);                             \
      fprintf(stdout, "  line: %d\n", __LINE__);                               \
      fprintf(stdout, "  ...\n");                                              \
    }                                                                          \
  } while (0)

#define TEST_FINISH()                                                          \
  do {                                                                         \
    int exit_code = 0;                                                         \
    if (__skene_test_count != __skene_planned_count) {                         \
      printf("Bail out! Plan mismatch: planned %d, ran %d\n",                  \
             __skene_planned_count, __skene_test_count);                       \
      exit_code = 1;                                                           \
    } else if (__skene_tests_failed > 0) {                                     \
      exit_code = 1;                                                           \
    }                                                                          \
    return exit_code;                                                          \
  } while (0)

PUBLIC int test_fixture_load(const char *path, char **out, size_t *len);

#endif

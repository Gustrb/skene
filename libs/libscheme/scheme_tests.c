#include "libarena/arena.h"
#include "libstrview/string_view.h"
#include <libtest/test.h>
#include <libscheme/scheme.h>

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void);

int main(void)
{
  TEST_START(3);
  __scheme_should_be_able_to_eval_simple_nums();
  TEST_FINISH();
  return 0;
}

#define BUFF_LEN 4096
static char __buff[BUFF_LEN] = {0};

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void)
{
  const char *program = "9";
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t result = scheme_eval(arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, result, "scheme shouldn't fail to eval a program that has just a number");
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, out.tag, "scheme should properly identify the type of the result");
  ASSERT_INT_EQ(9, out.as.val, "the value of a program that is just a constant should be said constant");
}



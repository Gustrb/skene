#include <libarena/arena.h>
#include <libstrbuilder/strbuilder.h>
#include <libstrview/string_view.h>
#include <libtest/test.h>
#include <libscheme/scheme.h>

#include <string.h>

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void);
PRIVATE void __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena_t arena, const char *program, const char *header, int32_t expected_result);

int main(void)
{
  TEST_START(15);
  __scheme_should_be_able_to_eval_simple_nums();
  TEST_FINISH();
  return 0;
}

#define BUFF_LEN 4096
static char __buff[BUFF_LEN] = {0};

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);
  __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena, "0", "[PROGRAM: \"0\"]: ", 0);
  __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena, "9", "[PROGRAM: \"9\"]: ", 9);
  __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena, "69", "[PROGRAM \"69\"]: ", 69);
  __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena, "420", "[PROGRAM \"420\"]: ", 420);
  __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena, "-69420", "[PROGRAM \"-69420\"]: ", -69420);
}

PRIVATE void __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena_t arena, const char *program, const char *header, int32_t expected_result)
{  

  scheme_value_t out = {0};
  int32_t result = scheme_eval(arena, string_view_from_cstr(program), &out);

  const char *messages[] = {
          "scheme shouldn't fail to eval a program that has just a number",
          "scheme should properly identify the type of the result",
          ("the value of a program that is just a constant should be said "
          "constant"),
  };

  const char *msg;
  string_builder_t builder = {0};

  // messages[2] is the largest one, so all will fit...
  string_builder_new_with_capacity(&builder, strlen(messages[2]) + strlen(header));

  {
    string_builder_append_cstr(&builder, header);
    string_builder_append_cstr(&builder, messages[0]);
    string_builder_into_owned_cstr_arena(&arena, &builder, &msg);
    ASSERT_INT_EQ(0, result, msg);
    builder.length = 0;
  }

  {
    string_builder_append_cstr(&builder, header);
    string_builder_append_cstr(&builder, messages[1]);
    string_builder_into_owned_cstr_arena(&arena, &builder, &msg);
    ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, out.tag, msg);
    builder.length = 0;
  }

  {
    string_builder_append_cstr(&builder, header);
    string_builder_append_cstr(&builder, messages[2]);
    string_builder_into_owned_cstr_arena(&arena, &builder, &msg);
    ASSERT_INT_EQ(expected_result, out.as.val, msg);
    builder.length = 0;
  }

  string_builder_destroy(&builder);
}



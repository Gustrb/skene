#include <libarena/arena.h>
#include <libstrbuilder/strbuilder.h>
#include <libstrview/string_view.h>
#include <libtest/test.h>
#include <libscheme/scheme.h>

#include <string.h>

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void);
PRIVATE void __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena_t arena, const char *program, const char *header, int32_t expected_result);
PRIVATE void __bug_that_already_happened_scheme_eval_should_have_a_heap_failure_when_reading_a_single_minus(void);

int main(void)
{
  TEST_START(16);
  __scheme_should_be_able_to_eval_simple_nums();
  __bug_that_already_happened_scheme_eval_should_have_a_heap_failure_when_reading_a_single_minus();
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
  int32_t result = scheme_eval(&arena, string_view_from_cstr(program), &out);

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

PRIVATE void __bug_that_already_happened_scheme_eval_should_have_a_heap_failure_when_reading_a_single_minus(void)
{
  char program[1];
  program[0] = '-';

  string_view_t program_sv = (string_view_t){.addr=program, .length=1};

  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t output = {0};
  int32_t err = scheme_eval(&arena, program_sv, &output);

  // TODO: currently we return an invalid token error, but that is temporary, this test was mostly to catch the Asan issue
  ASSERT_INT_EQ(1, err, "[Fixing a bug]: Evaluating a program that has only a '-' would trigger an access out of bounds");
  // TODO: assert that output.as.val is symbol
}

#include <libarena/arena.h>
#include <libstrbuilder/strbuilder.h>
#include <libstrview/string_view.h>
#include <libtest/test.h>
#include <libscheme/scheme.h>

#include <string.h>

PRIVATE void __scheme_should_be_able_to_eval_simple_nums(void);
PRIVATE void __scheme_eval_and_assert_equality_of_simple_numerical_programs(arena_t arena, const char *program, const char *header, int32_t expected_result);
PRIVATE void __bug_that_already_happened_scheme_eval_should_have_a_heap_failure_when_reading_a_single_minus(void);
PRIVATE void __scheme_should_reject_malformed_lists(void);
PRIVATE void __scheme_should_apply_procedures(void);
PRIVATE void __scheme_should_reject_non_applicable_heads(void);
PRIVATE void __scheme_should_evaluate_a_bare_primitive(void);
PRIVATE void __scheme_assert_eval_status(const char *program, int32_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_int(const char *program, int32_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_bool(const char *program, uint8_t expected, const char *desc);

PRIVATE void __scheme_should_evaluate_boolean_literals(void);
PRIVATE void __scheme_should_reject_malformed_boolean_literals(void);
PRIVATE void __scheme_should_compare_numbers(void);
PRIVATE void __scheme_if_should_evaluate_only_the_selected_branch(void);
PRIVATE void __scheme_should_reject_malformed_ifs(void);
PRIVATE void __scheme_define_should_bind_names(void);
PRIVATE void __scheme_should_reject_malformed_defines(void);
PRIVATE void __scheme_lambda_should_build_procedures(void);
PRIVATE void __scheme_should_reject_malformed_lambdas(void);
PRIVATE void __scheme_should_define_procedures(void);
PRIVATE void __scheme_closures_should_be_lexically_scoped(void);
PRIVATE void __scheme_procedures_should_recurse(void);
PRIVATE void __scheme_special_forms_should_not_be_values(void);
PRIVATE void __scheme_should_reject_wrongly_typed_arguments(void);
PRIVATE void __scheme_tail_calls_should_not_grow_the_stack(void);
PRIVATE void __scheme_non_tail_recursion_should_still_be_bounded(void);
PRIVATE void __scheme_tail_calls_should_preserve_semantics(void);
PRIVATE void __scheme_tail_loops_should_run_in_constant_memory(void);
PRIVATE void __scheme_frame_reuse_should_respect_captures(void);
PRIVATE void __scheme_frame_reuse_should_not_leak_stale_bindings(void);
PRIVATE void __scheme_cons_should_build_pairs(void);
PRIVATE void __scheme_cons_should_build_lists(void);
PRIVATE void __scheme_should_traverse_lists(void);
PRIVATE void __scheme_should_reject_bad_pair_operations(void);
PRIVATE void __scheme_predicates_should_classify_values(void);
PRIVATE void __scheme_nil_should_be_an_ordinary_binding(void);
PRIVATE void __scheme_conses_should_outlive_the_frame_that_made_them(void);
PRIVATE void __scheme_gc_should_reclaim_garbage(void);
PRIVATE void __scheme_gc_should_report_what_it_did(void);
PRIVATE void __scheme_gc_should_find_roots_on_the_c_stack(void);
PRIVATE void __scheme_gc_should_not_reclaim_live_data(void);
PRIVATE void __scheme_gc_should_honour_its_configuration(void);
PRIVATE void __scheme_gc_should_not_resurrect_recycled_slots(void);
PRIVATE void __scheme_assert_gc(const char *program, size_t arena_bytes, const scheme_config_t *config, int32_t expected_err, scheme_value_t *out, scheme_gc_stats_t *stats, const char *desc);
PRIVATE void __scheme_assert_gc_int(const char *program, size_t arena_bytes, const scheme_config_t *config, int32_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_int_list(const char *program, const int32_t *expected, size_t n, const char *desc);
PRIVATE void __scheme_assert_eval_tag(const char *program, scheme_value_tag_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_int_within(const char *program, size_t arena_bytes, int32_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_bool_within(const char *program, size_t arena_bytes, uint8_t expected, const char *desc);
PRIVATE void __scheme_assert_eval_status_within(const char *program, size_t arena_bytes, int32_t expected, const char *desc);

int main(void)
{
  TEST_START(631);
  __scheme_should_be_able_to_eval_simple_nums();
  __bug_that_already_happened_scheme_eval_should_have_a_heap_failure_when_reading_a_single_minus();
  __scheme_should_reject_malformed_lists();
  __scheme_should_apply_procedures();
  __scheme_should_reject_non_applicable_heads();
  __scheme_should_evaluate_a_bare_primitive();

  __scheme_should_evaluate_boolean_literals();
  __scheme_should_reject_malformed_boolean_literals();
  __scheme_should_compare_numbers();
  __scheme_if_should_evaluate_only_the_selected_branch();
  __scheme_should_reject_malformed_ifs();
  __scheme_define_should_bind_names();
  __scheme_should_reject_malformed_defines();
  __scheme_lambda_should_build_procedures();
  __scheme_should_reject_malformed_lambdas();
  __scheme_should_define_procedures();
  __scheme_closures_should_be_lexically_scoped();
  __scheme_procedures_should_recurse();
  __scheme_special_forms_should_not_be_values();
  __scheme_should_reject_wrongly_typed_arguments();

  __scheme_tail_calls_should_not_grow_the_stack();
  __scheme_non_tail_recursion_should_still_be_bounded();
  __scheme_tail_calls_should_preserve_semantics();
  __scheme_tail_loops_should_run_in_constant_memory();
  __scheme_frame_reuse_should_respect_captures();
  __scheme_frame_reuse_should_not_leak_stale_bindings();

  __scheme_cons_should_build_pairs();
  __scheme_cons_should_build_lists();
  __scheme_should_traverse_lists();
  __scheme_should_reject_bad_pair_operations();
  __scheme_predicates_should_classify_values();
  __scheme_nil_should_be_an_ordinary_binding();
  __scheme_conses_should_outlive_the_frame_that_made_them();

  __scheme_gc_should_reclaim_garbage();
  __scheme_gc_should_report_what_it_did();
  __scheme_gc_should_find_roots_on_the_c_stack();
  __scheme_gc_should_not_reclaim_live_data();
  __scheme_gc_should_honour_its_configuration();
  __scheme_gc_should_not_resurrect_recycled_slots();
  TEST_FINISH();
  return 0;
}

/* Nothing is ever reclaimed during evaluation. A tail loop recycles its frame
   and so costs nothing per iteration, but anything it *conses* stays, as does
   every frame a non-tail call makes. The list- and recursion-heavy tests below
   are what set this size — not the one-line programs. Tests that care about the
   exact cost use the `_within` helpers and their own budget. */
#define BUFF_LEN (1 << 23)
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

  // '-' now parses as a symbol and symbol eval is still a stub, so this
  // succeeds vacuously. This test exists to catch the ASan issue.
  ASSERT_INT_EQ(0, err, "[Fixing a bug]: Evaluating a program that has only a '-' would trigger an access out of bounds");
  // TODO: assert that output.as.val is symbol
}

PRIVATE void __scheme_assert_eval_status(const char *program, int32_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(expected, err, desc);
}

PRIVATE void __scheme_assert_eval_int(const char *program, int32_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, out.tag, desc);
  ASSERT_INT_EQ(expected, out.as.val, desc);
}

// A list is a procedure application, so only lists headed by a procedure can be
// evaluated. Argument order matters here: '-' and nesting are what catch a
// reversed cons chain.
PRIVATE void __scheme_should_apply_procedures(void)
{
  __scheme_assert_eval_int("(+ 1 2)", 3, "'+' should sum its arguments");
  __scheme_assert_eval_int("(+)", 0, "'+' with no arguments should be the additive identity");
  __scheme_assert_eval_int("(+ 1 2 3 4 5)", 15, "'+' should be variadic");
  __scheme_assert_eval_int("(*)", 1, "'*' with no arguments should be the multiplicative identity");
  __scheme_assert_eval_int("(* 2 3 4)", 24, "'*' should multiply its arguments");
  __scheme_assert_eval_int("(- 5)", -5, "'-' with one argument should negate");
  __scheme_assert_eval_int("(- 10 3)", 7, "'-' should subtract in argument order");
  __scheme_assert_eval_int("(- 10 3 2)", 5, "'-' should fold left, not right");
  __scheme_assert_eval_int("(+ 1 (* 2 3))", 7, "a nested application should be evaluated first");
  __scheme_assert_eval_int("(- (+ 10 5) (* 2 3))", 9, "nested applications in both positions");
  __scheme_assert_eval_int("(+ 1 2) (* 3 4)", 12, "the last top level expression should be the result");
  __scheme_assert_eval_int("-42", -42, "a negative literal should still be a number, not a symbol");
}

// The head of a list must evaluate to a procedure; a literal never does.
PRIVATE void __scheme_should_reject_non_applicable_heads(void)
{
  __scheme_assert_eval_status("()", ERR_SCHEME_NOT_APPLICABLE,
                              "the empty list has no procedure expression");
  __scheme_assert_eval_status("(1)", ERR_SCHEME_NOT_APPLICABLE,
                              "a number is not applicable");
  __scheme_assert_eval_status("(1 2 3)", ERR_SCHEME_NOT_APPLICABLE,
                              "a list of numbers is an application, not data");
  __scheme_assert_eval_status("(((1)))", ERR_SCHEME_NOT_APPLICABLE,
                              "the innermost application fails first");
  __scheme_assert_eval_status("(+ 1 (2 3))", ERR_SCHEME_NOT_APPLICABLE,
                              "a failing argument should abort the application");
  __scheme_assert_eval_status("(nope 1)", ERR_SCHEME_UNBOUND_SYMBOL,
                              "an unbound symbol should not resolve");
  __scheme_assert_eval_status("nope", ERR_SCHEME_UNBOUND_SYMBOL,
                              "a bare unbound symbol should not resolve");
  __scheme_assert_eval_status("(-)", ERR_SCHEME_BAD_ARITY,
                              "'-' requires at least one argument");
}

// '+' is a value, so it resolves on its own without being applied.
PRIVATE void __scheme_should_evaluate_a_bare_primitive(void)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr("+"), &out);

  ASSERT_INT_EQ(0, err, "a bare '+' should evaluate");
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_PRIMITIVE, out.tag, "a bare '+' should be a procedure value");
}

PRIVATE void __scheme_assert_eval_bool(const char *program, uint8_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_BOOL, out.tag, desc);
  ASSERT_INT_EQ(expected, out.as.boolean, desc);
}

PRIVATE void __scheme_should_evaluate_boolean_literals(void)
{
  __scheme_assert_eval_bool("#t", 1, "'#t' should be a true boolean");
  __scheme_assert_eval_bool("#f", 0, "'#f' should be a false boolean");
  __scheme_assert_eval_bool("(if #t #f #t)", 0, "a boolean should survive being a branch result");
}

// '#' introduces a literal that has to stand on its own; anything else is a
// typo rather than a boolean followed by something.
PRIVATE void __scheme_should_reject_malformed_boolean_literals(void)
{
  __scheme_assert_eval_status("#", ERR_SCHEME_INVALID_TOKEN,
                              "a bare '#' should be rejected");
  __scheme_assert_eval_status("#x", ERR_SCHEME_INVALID_TOKEN,
                              "'#x' is not a boolean literal");
  __scheme_assert_eval_status("#tx", ERR_SCHEME_INVALID_TOKEN,
                              "a boolean literal should not run into a symbol");
}

// Comparisons chain over adjacent pairs and yield booleans, which is what makes
// them usable as an 'if' test.
PRIVATE void __scheme_should_compare_numbers(void)
{
  __scheme_assert_eval_bool("(= 1 1)", 1, "'=' should hold for equal numbers");
  __scheme_assert_eval_bool("(= 1 2)", 0, "'=' should fail for different numbers");
  __scheme_assert_eval_bool("(= 1 1 1)", 1, "'=' should chain");
  __scheme_assert_eval_bool("(= 1 1 2)", 0, "'=' should fail if any adjacent pair differs");
  __scheme_assert_eval_bool("(< 1 2)", 1, "'<' should hold when ascending");
  __scheme_assert_eval_bool("(< 2 1)", 0, "'<' should fail when descending");
  __scheme_assert_eval_bool("(< 1 2 3)", 1, "'<' should chain over adjacent pairs");
  __scheme_assert_eval_bool("(< 1 3 2)", 0, "'<' should compare adjacent pairs, not just the ends");
  __scheme_assert_eval_bool("(> 3 2 1)", 1, "'>' should hold when descending");
  __scheme_assert_eval_bool("(<= 1 1 2)", 1, "'<=' should admit equal neighbours");
  __scheme_assert_eval_bool("(>= 2 2 1)", 1, "'>=' should admit equal neighbours");
  __scheme_assert_eval_bool("(< -2 -1)", 1, "'<' should order negative numbers");
  __scheme_assert_eval_bool("(= (+ 1 2) 3)", 1, "a comparison should evaluate its arguments");

  __scheme_assert_eval_status("(=)", ERR_SCHEME_BAD_ARITY,
                              "a comparison needs two arguments to compare");
  __scheme_assert_eval_status("(< 1)", ERR_SCHEME_BAD_ARITY,
                              "one argument leaves no pair to compare");
}

/* The whole point of 'if' being a special form: the branch that is not selected
   is never evaluated. '(nope)' would be an unbound symbol if it were, so these
   succeeding is the proof. */
PRIVATE void __scheme_if_should_evaluate_only_the_selected_branch(void)
{
  __scheme_assert_eval_int("(if #t 1 2)", 1, "a true test should select the consequent");
  __scheme_assert_eval_int("(if #f 1 2)", 2, "a false test should select the alternative");
  __scheme_assert_eval_int("(if #t 1 (nope))", 1,
                           "the alternative should not be evaluated when the test is true");
  __scheme_assert_eval_int("(if #f (nope) 2)", 2,
                           "the consequent should not be evaluated when the test is false");
  __scheme_assert_eval_int("(if (< 1 2) 10 20)", 10, "the test itself should be evaluated");
  __scheme_assert_eval_int("(if (> 1 2) 10 20)", 20, "a computed false test should select the alternative");
  __scheme_assert_eval_int("(if #t 1)", 1, "a one armed 'if' should yield its consequent");
  __scheme_assert_eval_bool("(if #f 1)", 0,
                            "a one armed 'if' whose test fails should still have a value");

  // Only #f is false, so 0 and the empty-ish values are all truthy.
  __scheme_assert_eval_int("(if 0 1 2)", 1, "zero should be truthy, only '#f' is false");
  __scheme_assert_eval_int("(if -1 1 2)", 1, "a negative number should be truthy");
  __scheme_assert_eval_int("(if + 1 2)", 1, "a procedure should be truthy");

  __scheme_assert_eval_int("(if #t (if #f 1 2) 3)", 2, "'if' should nest");
}

PRIVATE void __scheme_should_reject_malformed_ifs(void)
{
  __scheme_assert_eval_status("(if)", ERR_SCHEME_BAD_SYNTAX,
                              "'if' needs a test and a consequent");
  __scheme_assert_eval_status("(if #t)", ERR_SCHEME_BAD_SYNTAX,
                              "'if' needs a consequent");
  __scheme_assert_eval_status("(if #t 1 2 3)", ERR_SCHEME_BAD_SYNTAX,
                              "'if' takes at most three operands");
}

PRIVATE void __scheme_define_should_bind_names(void)
{
  __scheme_assert_eval_int("(define x 10) x", 10, "a defined name should resolve to its value");
  __scheme_assert_eval_int("(define x 10)", 10, "'define' should evaluate to the bound value");
  __scheme_assert_eval_int("(define x (+ 1 2)) (* x 2)", 6,
                           "'define' should evaluate its value expression");
  __scheme_assert_eval_int("(define x 1) (define x 2) x", 2,
                           "re-defining a name should replace the binding");
  __scheme_assert_eval_int("(define x 1) (define y 2) (+ x y)", 3,
                           "several bindings should coexist");
  __scheme_assert_eval_int("(define x 1) (define y x) y", 1,
                           "a definition should see the bindings before it");
  __scheme_assert_eval_bool("(define x #t) x", 1, "a binding should keep its type");
}

// The name in a 'define' is not evaluated, so the only way to get these wrong
// is structurally.
PRIVATE void __scheme_should_reject_malformed_defines(void)
{
  __scheme_assert_eval_status("(define)", ERR_SCHEME_BAD_SYNTAX,
                              "'define' needs a target");
  __scheme_assert_eval_status("(define x)", ERR_SCHEME_BAD_SYNTAX,
                              "'define' needs a value expression");
  __scheme_assert_eval_status("(define x 1 2)", ERR_SCHEME_BAD_SYNTAX,
                              "'define' takes exactly one value expression");
  __scheme_assert_eval_status("(define 1 2)", ERR_SCHEME_BAD_SYNTAX,
                              "a number is not a bindable name");
  __scheme_assert_eval_status("(define () 1)", ERR_SCHEME_BAD_SYNTAX,
                              "an empty target list has no procedure name");
  __scheme_assert_eval_status("(define (1 x) x)", ERR_SCHEME_BAD_SYNTAX,
                              "a procedure definition needs a symbol for a name");
}

PRIVATE void __scheme_lambda_should_build_procedures(void)
{
  __scheme_assert_eval_int("((lambda (x) x) 42)", 42, "a lambda applied to an argument should return it");
  __scheme_assert_eval_int("((lambda (x y) (- x y)) 10 3)", 7, "parameters should bind in order");
  __scheme_assert_eval_int("((lambda () 7))", 7, "a nullary lambda should be applicable");
  __scheme_assert_eval_int("((lambda (x) (* x x)) 6)", 36, "a lambda body may be an application");
  __scheme_assert_eval_int("((lambda (x) (+ x 1) (* x 2)) 5)", 10,
                           "a multi expression body should yield the last value");
  __scheme_assert_eval_int("(((lambda (x) (lambda (y) (+ x y))) 3) 4)", 7,
                           "a lambda should be able to return a lambda");

  __scheme_assert_eval_status("((lambda (x) x))", ERR_SCHEME_BAD_ARITY,
                              "too few arguments should be rejected");
  __scheme_assert_eval_status("((lambda (x) x) 1 2)", ERR_SCHEME_BAD_ARITY,
                              "too many arguments should be rejected");

  // A lambda is a value, so it resolves without being applied.
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr("(lambda (x) x)"), &out);

  ASSERT_INT_EQ(0, err, "an unapplied lambda should evaluate");
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_CLOSURE, out.tag, "an unapplied lambda should be a closure value");
}

PRIVATE void __scheme_should_reject_malformed_lambdas(void)
{
  __scheme_assert_eval_status("(lambda)", ERR_SCHEME_BAD_SYNTAX,
                              "'lambda' needs a parameter list");
  __scheme_assert_eval_status("(lambda (x))", ERR_SCHEME_BAD_SYNTAX,
                              "'lambda' needs at least one body expression");
  __scheme_assert_eval_status("(lambda (1) x)", ERR_SCHEME_BAD_SYNTAX,
                              "a parameter must be a symbol");
  __scheme_assert_eval_status("(lambda x x)", ERR_SCHEME_BAD_SYNTAX,
                              "a bare symbol is not a parameter list, there is no variadic form yet");
  __scheme_assert_eval_status("(lambda ((x)) x)", ERR_SCHEME_BAD_SYNTAX,
                              "a nested list is not a parameter");
}

// '(define (f x) ...)' is sugar for binding a lambda, so it should agree with
// the explicit form in every respect.
PRIVATE void __scheme_should_define_procedures(void)
{
  __scheme_assert_eval_int("(define square (lambda (x) (* x x))) (square 5)", 25,
                           "a lambda bound by 'define' should be applicable");
  __scheme_assert_eval_int("(define (square x) (* x x)) (square 5)", 25,
                           "the procedure definition shorthand should agree with the lambda form");
  __scheme_assert_eval_int("(define (f) 42) (f)", 42,
                           "a nullary procedure should be definable");
  __scheme_assert_eval_int("(define (sum3 a b c) (+ a b c)) (sum3 1 2 3)", 6,
                           "several parameters should bind in order");
  __scheme_assert_eval_int("(define (f x) (+ x 1) (* x 2)) (f 5)", 10,
                           "the shorthand should accept a multi expression body");
  __scheme_assert_eval_int("(define (double x) (* x 2)) (double (double 3))", 12,
                           "a procedure should compose with itself");
  __scheme_assert_eval_status("(define (f x) x) (f)", ERR_SCHEME_BAD_ARITY,
                              "a defined procedure should still check its arity");
}

/* A closure captures the environment it was *written* in, not the one it is
   called from. The make-adder and dynamic-scope cases below are the ones that
   would pass under naive substitution and fail under dynamic scope. */
PRIVATE void __scheme_closures_should_be_lexically_scoped(void)
{
  __scheme_assert_eval_int("(define x 5) (define (f y) (+ x y)) (f 3)", 8,
                           "a procedure body should see the enclosing scope");
  __scheme_assert_eval_int(
      "(define (make-adder n) (lambda (x) (+ x n)))"
      "(define add5 (make-adder 5))"
      "(add5 10)",
      15, "a returned lambda should keep the captured parameter alive");
  __scheme_assert_eval_int(
      "(define (make-adder n) (lambda (x) (+ x n)))"
      "(define add2 (make-adder 2))"
      "(define add3 (make-adder 3))"
      "(+ (add2 10) (add3 10))",
      25, "two closures over the same lambda should capture separate bindings");
  __scheme_assert_eval_int(
      "(define x 10) (define (f) x) (define (g x) (f)) (g 99)",
      10, "a callee should not see the caller's bindings, scope is lexical not dynamic");
  __scheme_assert_eval_int("(define x 1) (define (f x) x) (f 2)", 2,
                           "a parameter should shadow an outer binding");
  __scheme_assert_eval_int("(define x 1) (define (f x) x) (f 2) x", 1,
                           "shadowing should not leak back into the outer scope");
  __scheme_assert_eval_int("(define (f x) (define y (* x 2)) (+ x y)) (f 3)", 9,
                           "'define' inside a body should bind in the call frame");
  __scheme_assert_eval_int("(define (f x) (define y (* x 2)) (+ x y)) (f 3) (f 4)", 12,
                           "a call frame should not be reused between calls");
}

PRIVATE void __scheme_procedures_should_recurse(void)
{
  // The binding is installed in the same env the closure captured, which is
  // what lets the body find its own name.
  __scheme_assert_eval_int(
      "(define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))"
      "(fact 0)",
      1, "a recursive procedure should return its base case");
  __scheme_assert_eval_int(
      "(define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))"
      "(fact 5)",
      120, "a recursive procedure should see its own name");
  __scheme_assert_eval_int(
      "(define (fact n) (if (= n 0) 1 (* n (fact (- n 1)))))"
      "(fact 10)",
      3628800, "recursion should hold up over more activations");
  __scheme_assert_eval_int(
      "(define (fib n) (if (< n 2) n (+ (fib (- n 1)) (fib (- n 2)))))"
      "(fib 10)",
      55, "a doubly recursive procedure should work");
  __scheme_assert_eval_int(
      "(define (even n) (if (= n 0) #t (odd (- n 1))))"
      "(define (odd n) (if (= n 0) #f (even (- n 1))))"
      "(if (even 10) 1 0)",
      1, "mutual recursion should resolve, the second name is bound by call time");

  /* There is deliberately no test for `(define (loop) (loop)) (loop)` here.
     That call is in tail position and its frame is recycled, so it consumes
     neither stack nor arena: it is a true infinite loop, exactly as the same
     program is in any Scheme, and there is nothing for a test to wait for. A
     step budget is the only thing that could bound it, and the evaluator
     deliberately has none. */
}

/* Everything below is about tail calls. __SCHEME_MAX_DEPTH is 256, so every
   loop here is an order of magnitude past what the evaluator could do if tail
   positions still cost a C frame. A regression would show up as
   ERR_SCHEME_DEPTH_EXCEEDED, not as a wrong answer. */
PRIVATE void __scheme_tail_calls_should_not_grow_the_stack(void)
{
  // The plainest shape: self tail call in the alternative of an 'if'.
  __scheme_assert_eval_int(
      "(define (count n) (if (= n 0) 0 (count (- n 1))))"
      "(count 1000)",
      0, "a self tail call should loop rather than recurse");
  __scheme_assert_eval_int(
      "(define (count n) (if (= n 0) 0 (count (- n 1))))"
      "(count 10000)",
      0, "a tail loop should be bounded by the arena, not by the stack");

  // ...and in the consequent, which is a separate branch of the same dispatch.
  __scheme_assert_eval_int(
      "(define (count n) (if (> n 0) (count (- n 1)) 0))"
      "(count 1000)",
      0, "a tail call in the consequent should also loop");

  // An accumulator threaded through the loop is the idiom TCO exists to serve.
  __scheme_assert_eval_int(
      "(define (sum n acc) (if (= n 0) acc (sum (- n 1) (+ acc n))))"
      "(sum 1000 0)",
      500500, "an accumulator should survive a long tail loop");
  __scheme_assert_eval_int(
      "(define (count n acc) (if (= n 0) acc (count (- n 1) (* acc 1))))"
      "(count 2000 7)",
      7, "a tail loop should not disturb an argument it merely passes along");

  // Crossing between two closures, so the tail call is not a self call.
  __scheme_assert_eval_bool(
      "(define (even n) (if (= n 0) #t (odd (- n 1))))"
      "(define (odd n) (if (= n 0) #f (even (- n 1))))"
      "(even 1000)",
      1, "mutual tail calls should loop, not nest");
  __scheme_assert_eval_bool(
      "(define (even n) (if (= n 0) #t (odd (- n 1))))"
      "(define (odd n) (if (= n 0) #f (even (- n 1))))"
      "(even 1001)",
      0, "a mutual tail loop should still compute the right answer");

  // The 'define' shorthand and an explicit lambda must optimise alike.
  __scheme_assert_eval_int(
      "(define count (lambda (n) (if (= n 0) 0 (count (- n 1)))))"
      "(count 1000)",
      0, "a lambda bound by 'define' should get the same tail treatment");

  /* Only the *last* body expression is in tail position. The earlier ones must
     still run, and must not cost the loop its flatness. */
  __scheme_assert_eval_int(
      "(define (count n) (define x 1) (if (= n 0) 0 (count (- n 1))))"
      "(count 500)",
      0, "the last expression of a multi expression body should be the tail position");

  // A tail loop nested inside a non-tail context still gets its own trampoline.
  __scheme_assert_eval_int(
      "(define (count n) (if (= n 0) 0 (count (- n 1))))"
      "(+ 1 (count 1000))",
      1, "a tail loop should work when it is itself an argument");
  __scheme_assert_eval_int(
      "(define (count n) (if (= n 0) 0 (count (- n 1))))"
      "(+ (count 1000) (count 1000))",
      0, "two tail loops in one application should not accumulate depth");
}

/* TCO must not quietly turn every call into a loop: an expression whose value
   is consumed by its caller still has work pending, so it still nests. */
PRIVATE void __scheme_non_tail_recursion_should_still_be_bounded(void)
{
  // '(f ...)' sits in an argument of '+', so it is not in tail position.
  __scheme_assert_eval_int(
      "(define (f n) (if (= n 0) 0 (+ 0 (f (- n 1)))))"
      "(f 10)",
      0, "shallow non tail recursion should still work");
  __scheme_assert_eval_int(
      "(define (f n) (if (= n 0) 0 (+ 0 (f (- n 1)))))"
      "(f 100)",
      0, "non tail recursion should work up to the depth limit");
  __scheme_assert_eval_status(
      "(define (f n) (if (= n 0) 0 (+ 0 (f (- n 1)))))"
      "(f 1000)",
      ERR_SCHEME_DEPTH_EXCEEDED,
      "non tail recursion should still be caught by the depth limit");

  // The operator position is a subexpression too.
  __scheme_assert_eval_status(
      "(define (f n) ((f n)))"
      "(f 1)",
      ERR_SCHEME_DEPTH_EXCEEDED,
      "recursion through the operator position should be bounded");

  // A value expression of 'define' is consumed by the binding, so it nests.
  __scheme_assert_eval_status(
      "(define (f n) (define x (f n)) x)"
      "(f 1)",
      ERR_SCHEME_DEPTH_EXCEEDED,
      "recursion through a 'define' value should be bounded");

  // The test of an 'if' is consumed by the branch selection, so it nests.
  __scheme_assert_eval_status(
      "(define (f n) (if (f n) 1 2))"
      "(f 1)",
      ERR_SCHEME_DEPTH_EXCEEDED,
      "recursion through an 'if' test should be bounded");

  // Non-last body expressions are evaluated for effect, not in tail position.
  __scheme_assert_eval_status(
      "(define (f n) (f n) 1)"
      "(f 1)",
      ERR_SCHEME_DEPTH_EXCEEDED,
      "recursion through a non final body expression should be bounded");
}

/* The trampoline rewrites control flow, so the things that ride along with it —
   environments, closures, error propagation — need to come out unchanged. */
PRIVATE void __scheme_tail_calls_should_preserve_semantics(void)
{
  // An enclosing binding must still resolve after many iterations.
  __scheme_assert_eval_int(
      "(define x 100)"
      "(define (count n) (if (= n 0) x (count (- n 1))))"
      "(count 500)",
      100, "the captured scope should survive a long tail loop");
  __scheme_assert_eval_int(
      "(define (count n x) (if (= n 0) x (count (- n 1) x)))"
      "(count 500 7)",
      7, "a shadowing parameter should survive a long tail loop");

  // A closure built before the loop must still be callable from inside it.
  __scheme_assert_eval_int(
      "(define (make n) (lambda () n))"
      "(define g (make 42))"
      "(define (count n) (if (= n 0) (g) (count (- n 1))))"
      "(count 500)",
      42, "a closure captured before a tail loop should not be clobbered by it");

  // Errors raised deep inside a loop have to escape the trampoline.
  __scheme_assert_eval_status(
      "(define (count n) (if (= n 0) (nope) (count (- n 1))))"
      "(count 500)",
      ERR_SCHEME_UNBOUND_SYMBOL,
      "an error at the end of a tail loop should propagate out");
  __scheme_assert_eval_status(
      "(define (count n) (if (= n 0) (+ #t 1) (count (- n 1))))"
      "(count 500)",
      ERR_SCHEME_TYPE_ERROR,
      "a type error at the end of a tail loop should propagate out");
  __scheme_assert_eval_status(
      "(define (f) (g))"
      "(define (g x) x)"
      "(f)",
      ERR_SCHEME_BAD_ARITY,
      "an arity error in tail position should still be reported");

  // A non-final body expression that fails must abort the call, which proves
  // it was evaluated at all.
  __scheme_assert_eval_status("(define (f) (nope) 1) (f)", ERR_SCHEME_UNBOUND_SYMBOL,
                              "a failing non final body expression should abort the call");

  // 'if' in a tail position still produces ordinary values.
  __scheme_assert_eval_int("(define (f) (if #t 42 0)) (f)", 42,
                           "a tail 'if' should yield its branch value");
  __scheme_assert_eval_bool("(define (f) (if #f 1)) (f)", 0,
                            "a one armed tail 'if' should still have a value");
  __scheme_assert_eval_int("(define (f) (if #t (if #f 1 2) 3)) (f)", 2,
                           "tail positions should nest");
  __scheme_assert_eval_int("(+ 1 (if #t 2 3))", 3,
                           "'if' in a non tail position should still return a value");

  // A closure may be produced from tail position and applied afterwards.
  __scheme_assert_eval_int("(define (f) (lambda (x) x)) ((f) 5)", 5,
                           "a closure returned from tail position should be applicable");
  __scheme_assert_eval_int(
      "(define (pick n) (if (= n 0) (lambda (x) (* x 2)) (pick (- n 1))))"
      "((pick 500) 21)",
      42, "a closure returned from the end of a tail loop should be applicable");

  // A tail call to a primitive is not tail work; its value is returned directly.
  __scheme_assert_eval_int("(define (f x) (+ x 1)) (f 1)", 2,
                           "a primitive in tail position should return its value");
}

/* The `_within` helpers cap the arena instead of handing over the whole buffer.
   That turns "how much did this program allocate" into something a test can
   assert: a tail loop that recycles its frame allocates nothing per iteration,
   so the bound it fits in does not move when the iteration count does. Even 16
   bytes of per-iteration garbage would blow these budgets.

   The budgets below are the measured floor rounded up one power of two, so they
   fail loudly on a regression without being brittle about small fixed costs.
   They include one heap page — the collector takes a whole page from the arena
   the first time anything is allocated, and a closure counts, so even a program
   that never conses pays for it. That is a fixed cost, not a per-iteration one,
   which is why these still assert flatness across four orders of magnitude. */
#define CONSTANT_BUDGET 8192
#define CONSTANT_BUDGET_TWO_PROCS 16384

PRIVATE void __scheme_assert_eval_int_within(const char *program, size_t arena_bytes, int32_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, (ptrdiff_t)arena_bytes);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, out.tag, desc);
  ASSERT_INT_EQ(expected, out.as.val, desc);
}

PRIVATE void __scheme_assert_eval_bool_within(const char *program, size_t arena_bytes, uint8_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, (ptrdiff_t)arena_bytes);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_BOOL, out.tag, desc);
  ASSERT_INT_EQ(expected, out.as.boolean, desc);
}

PRIVATE void __scheme_assert_eval_status_within(const char *program, size_t arena_bytes, int32_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, (ptrdiff_t)arena_bytes);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(expected, err, desc);
}

PRIVATE void __scheme_tail_loops_should_run_in_constant_memory(void)
{
  /* The same program, the same arena, four orders of magnitude apart in
     iterations. This is the whole claim of frame reuse: without it the third
     line needs tens of megabytes, because each iteration would bump a fresh
     environment out of the arena and never give it back. */
  __scheme_assert_eval_int_within(
      "(define (count n) (if (= n 0) 0 (count (- n 1)))) (count 100)",
      CONSTANT_BUDGET, 0, "a short tail loop should fit in a small arena");
  __scheme_assert_eval_int_within(
      "(define (count n) (if (= n 0) 0 (count (- n 1)))) (count 10000)",
      CONSTANT_BUDGET, 0, "a longer tail loop should fit in the same arena");
  __scheme_assert_eval_int_within(
      "(define (count n) (if (= n 0) 0 (count (- n 1)))) (count 1000000)",
      CONSTANT_BUDGET, 0, "a million iterations should fit in the same arena as a hundred");

  // An accumulator is rebound every iteration, so it has to be rebound in place
  // rather than boxed afresh.
  __scheme_assert_eval_int_within(
      "(define (sum n acc) (if (= n 0) acc (sum (- n 1) (+ acc n)))) (sum 100 0)",
      CONSTANT_BUDGET, 5050, "an accumulator loop should fit in a small arena");
  __scheme_assert_eval_int_within(
      "(define (sum n acc) (if (= n 0) acc (sum (- n 1) (+ acc n)))) (sum 10000 0)",
      CONSTANT_BUDGET, 50005000, "a long accumulator loop should fit in the same arena");

  // Bound by 'define' shorthand or by an explicit lambda, it is the same frame.
  __scheme_assert_eval_int_within(
      "(define count (lambda (n) (if (= n 0) 0 (count (- n 1))))) (count 100000)",
      CONSTANT_BUDGET, 0, "a lambda bound loop should also run in constant memory");

  /* Two different procedures taking turns. The frame changes hands every
     iteration, so this only stays flat because the parameter names line up and
     the bindings are overwritten rather than cleared and re-boxed. */
  __scheme_assert_eval_bool_within(
      "(define (even n) (if (= n 0) #t (odd (- n 1))))"
      "(define (odd n) (if (= n 0) #f (even (- n 1))))"
      "(even 100)",
      CONSTANT_BUDGET_TWO_PROCS, 1, "a mutual tail loop should fit in a small arena");
  __scheme_assert_eval_bool_within(
      "(define (even n) (if (= n 0) #t (odd (- n 1))))"
      "(define (odd n) (if (= n 0) #f (even (- n 1))))"
      "(even 100000)",
      CONSTANT_BUDGET_TWO_PROCS, 1, "a long mutual tail loop should fit in the same arena");
}

/* A frame may only be recycled while nothing else can still reach it. These are
   the shapes where something can, and where reuse therefore has to be declined
   — a wrong answer here means a closure was left pointing at storage that a
   later call overwrote. */
PRIVATE void __scheme_frame_reuse_should_respect_captures(void)
{
  /* Each iteration closes over its own frame, so each iteration needs a frame
     of its own. The answer is 1, not 0 and not 5: the surviving closure is the
     one built on the last iteration before the base case, where n was 1. */
  __scheme_assert_eval_int(
      "(define (f n acc) (if (= n 0) (acc) (f (- n 1) (lambda () n))))"
      "(f 5 (lambda () 0))",
      1, "a closure made inside a loop should keep the frame it captured");
  __scheme_assert_eval_int(
      "(define (f n acc) (if (= n 0) (acc) (f (- n 1) (lambda () n))))"
      "(f 100 (lambda () 0))",
      1, "the captured frame should survive a long loop");

  // The capturing frame here belongs to the helper, not to the loop, so the
  // loop frame stays recyclable while the helper's does not.
  __scheme_assert_eval_int(
      "(define (make n) (lambda () n))"
      "(define (loop n acc) (if (= n 0) (acc) (loop (- n 1) (make n))))"
      "(loop 100 (make 0))",
      1, "a closure built by a helper should outlive the loop that called it");

  // A closure defined in the body captures the frame it is defined in, so that
  // frame cannot be handed to the tail call that follows.
  __scheme_assert_eval_int(
      "(define (f n) (define (g) n) (if (= n 0) (g) (f (- n 1))))"
      "(f 50)",
      0, "a body defined closure should pin its frame");

  // Built before the loop, called after it.
  __scheme_assert_eval_int(
      "(define (make n) (lambda () n))"
      "(define g (make 7))"
      "(define (count n) (if (= n 0) (g) (count (- n 1))))"
      "(count 1000)",
      7, "a closure made before a loop should be unaffected by it");

  /* The inverse of the constant-memory tests, and what stops those from passing
     vacuously: a loop that captures every frame must *fail* to fit the budget a
     recycling loop fits in. If reuse ever stopped checking `captured`, this
     would start passing and the correctness tests above would start failing. */
  __scheme_assert_eval_status_within(
      "(define (f n acc) (if (= n 0) (acc) (f (- n 1) (lambda () n))))"
      "(f 1000 (lambda () 0))",
      CONSTANT_BUDGET, ERR_SCHEME_NOMEM,
      "a loop that captures every frame should not run in constant memory");
}

/* Recycling a frame for a different callee has to leave no trace of the last
   one. A stale binding would not crash — it would silently shadow an outer
   scope, which is worse. */
PRIVATE void __scheme_frame_reuse_should_not_leak_stale_bindings(void)
{
  // 'b' belongs to the caller's parameter list and must not be visible in a
  // callee that never declared it.
  __scheme_assert_eval_status(
      "(define (two a b) (one a))"
      "(define (one a) b)"
      "(two 3 4)",
      ERR_SCHEME_UNBOUND_SYMBOL,
      "a parameter of the previous callee should not leak into the next");

  // The nastier version: the stale name exists in an outer scope, so a leak
  // shows up as the wrong value rather than an error.
  __scheme_assert_eval_int(
      "(define x 99)"
      "(define (two a x) (one a))"
      "(define (one a) x)"
      "(two 1 2)",
      99, "a stale binding should not shadow an outer one");

  // A name introduced by a body 'define' is not a parameter, so it must go too.
  __scheme_assert_eval_status(
      "(define (f n) (define z 5) (g n))"
      "(define (g n) z)"
      "(f 1)",
      ERR_SCHEME_UNBOUND_SYMBOL,
      "a body definition should not leak into a tail called procedure");

  // Arity changing in both directions across a recycled frame.
  __scheme_assert_eval_int(
      "(define (one a) (two a a))"
      "(define (two a b) (+ a b))"
      "(one 5)",
      10, "a frame should be reusable by a callee that takes more parameters");
  __scheme_assert_eval_int(
      "(define (two a b) (one a))"
      "(define (one a) a)"
      "(two 3 4)",
      3, "a frame should be reusable by a callee that takes fewer parameters");

  // Same arity, different names.
  __scheme_assert_eval_int(
      "(define (a n) (if (= n 0) 0 (bb (- n 1))))"
      "(define (bb m) (a m))"
      "(a 100)",
      0, "a frame should be reusable by a callee whose parameter is named differently");
}

/* Special forms are matched textually before evaluation, so their names are
   keywords rather than bindings: there is nothing in the environment to find. */
PRIVATE void __scheme_special_forms_should_not_be_values(void)
{
  __scheme_assert_eval_status("if", ERR_SCHEME_UNBOUND_SYMBOL,
                              "'if' is a keyword, not a value");
  __scheme_assert_eval_status("define", ERR_SCHEME_UNBOUND_SYMBOL,
                              "'define' is a keyword, not a value");
  __scheme_assert_eval_status("lambda", ERR_SCHEME_UNBOUND_SYMBOL,
                              "'lambda' is a keyword, not a value");
}

PRIVATE void __scheme_should_reject_wrongly_typed_arguments(void)
{
  __scheme_assert_eval_status("(+ #t 1)", ERR_SCHEME_TYPE_ERROR,
                              "arithmetic should reject a boolean");
  __scheme_assert_eval_status("(< 1 #f)", ERR_SCHEME_TYPE_ERROR,
                              "a comparison should reject a boolean");
  __scheme_assert_eval_status("(+ 1 (lambda (x) x))", ERR_SCHEME_TYPE_ERROR,
                              "arithmetic should reject a closure");
  __scheme_assert_eval_status("(#t)", ERR_SCHEME_NOT_APPLICABLE,
                              "a boolean is not applicable");
}

PRIVATE void __scheme_should_reject_malformed_lists(void)
{
  __scheme_assert_eval_status(")", ERR_SCHEME_UNEXPECTED_RPAREN,
                              "a stray ')' should be rejected");
  __scheme_assert_eval_status("(", ERR_SCHEME_UNTERMINATED_LIST,
                              "a lone '(' should be an unterminated list");
  __scheme_assert_eval_status("(1 2", ERR_SCHEME_UNTERMINATED_LIST,
                              "a list running off the end should be rejected");
  __scheme_assert_eval_status("(1 (2", ERR_SCHEME_UNTERMINATED_LIST,
                              "an unterminated inner list should be rejected");
}

/* Asserts that `program` evaluates to a proper list of exactly `n` integers.
   Walking with scheme_cdr rather than reaching into the cell is the point: it
   is the same interface a caller has, and it is what would keep working if
   pairs ever moved. The terminator is checked too — a chain that ends in
   anything but nil is an improper list, which is a different value. */
PRIVATE void __scheme_assert_eval_int_list(const char *program, const int32_t *expected, size_t n, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);

  scheme_value_t cursor = out;
  for (size_t i = 0; i < n; ++i)
  {
    ASSERT_INT_EQ(SCHEME_VALUE_TAG_PAIR, cursor.tag, desc);

    scheme_value_t head = {0};
    ASSERT_INT_EQ(0, scheme_car(cursor, &head), desc);
    ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, head.tag, desc);
    ASSERT_INT_EQ(expected[i], head.as.val, desc);

    ASSERT_INT_EQ(0, scheme_cdr(cursor, &cursor), desc);
  }

  ASSERT_INT_EQ(SCHEME_VALUE_TAG_NIL, cursor.tag, desc);
}

PRIVATE void __scheme_assert_eval_tag(const char *program, scheme_value_tag_t expected, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, BUFF_LEN);

  scheme_value_t out = {0};
  int32_t err = scheme_eval(&arena, string_view_from_cstr(program), &out);

  ASSERT_INT_EQ(0, err, desc);
  ASSERT_INT_EQ(expected, out.tag, desc);
}

PRIVATE void __scheme_cons_should_build_pairs(void)
{
  __scheme_assert_eval_tag("(cons 1 2)", SCHEME_VALUE_TAG_PAIR,
                           "'cons' should produce a pair");
  __scheme_assert_eval_int("(car (cons 1 2))", 1, "'car' should return the first field");
  __scheme_assert_eval_int("(cdr (cons 1 2))", 2, "'cdr' should return the second field");

  /* An improper pair: the cdr is not a list. This is legal, and distinguishing
     it from a one-element list is the whole reason cons is not `list`. */
  __scheme_assert_eval_tag("(cdr (cons 1 2))", SCHEME_VALUE_TAG_INT,
                           "the cdr of an improper pair should not be a list");
  __scheme_assert_eval_tag("(cdr (cons 1 nil))", SCHEME_VALUE_TAG_NIL,
                           "the cdr of a one-element list should be nil");

  __scheme_assert_eval_int("(car (cons (+ 1 2) 4))", 3,
                           "'cons' should receive its arguments already evaluated");
  __scheme_assert_eval_bool("(car (cons #t 1))", 1, "a pair should hold a boolean");
  __scheme_assert_eval_tag("(car (cons (lambda (x) x) 1))", SCHEME_VALUE_TAG_CLOSURE,
                           "a pair should hold a closure");
  __scheme_assert_eval_int("((car (cons (lambda (x) (* x 2)) 1)) 21)", 42,
                           "a closure retrieved from a pair should still be applicable");

  __scheme_assert_eval_tag("(car (cons (cons 1 2) 3))", SCHEME_VALUE_TAG_PAIR,
                           "a pair should nest in the car");
  __scheme_assert_eval_int("(car (car (cons (cons 1 2) 3)))", 1,
                           "a nested pair should be reachable through two cars");
  __scheme_assert_eval_int("(cdr (car (cons (cons 1 2) 3)))", 2,
                           "a nested pair should keep its own cdr");
}

PRIVATE void __scheme_cons_should_build_lists(void)
{
  const int32_t one[]   = { 1 };
  const int32_t three[] = { 1, 2, 3 };
  const int32_t nested[] = { 10, 20 };

  __scheme_assert_eval_tag("nil", SCHEME_VALUE_TAG_NIL, "'nil' should be the empty list");
  __scheme_assert_eval_int_list("(cons 1 nil)", one, 1, "a one-element list");
  __scheme_assert_eval_int_list("(cons 1 (cons 2 (cons 3 nil)))", three, 3,
                                "a three-element list should be built right to left");
  __scheme_assert_eval_int_list("(cons (- 11 1) (cons (* 4 5) nil))", nested, 2,
                                "list elements should be evaluated");

  /* Built by a procedure rather than written literally, so the cells outlive
     the frame that made them. That is the property a collector will have to
     respect, and it is worth pinning now. */
  __scheme_assert_eval_int_list(
    "(define (upto n) (if (= n 0) nil (cons n (upto (- n 1)))))"
    "(upto 3)",
    (const int32_t[]){ 3, 2, 1 }, 3,
    "a list built by a recursive procedure should survive the calls that made it");
}

#define LENGTH "(define (length xs) (if (null? xs) 0 (+ 1 (length (cdr xs)))))"
#define SUM    "(define (sum xs acc) (if (null? xs) acc (sum (cdr xs) (+ acc (car xs)))))"

PRIVATE void __scheme_should_traverse_lists(void)
{
  __scheme_assert_eval_int(LENGTH "(length nil)", 0,
                           "the empty list should have length 0");

  /* Non-tail recursion over a list: every cell must still be reachable on the
     way back up, after the deepest call has returned. */
  __scheme_assert_eval_int(LENGTH "(length (cons 1 (cons 2 (cons 3 nil))))", 3,
                           "'length' should count a three-element list");

  /* Tail recursion over a list: the frame is recycled on every iteration, so
     this is also a check that recycling does not disturb the pairs the loop is
     walking. They live in the heap, not in the frame. */
  __scheme_assert_eval_int(SUM "(sum (cons 1 (cons 2 (cons 3 (cons 4 nil)))) 0)", 10,
                           "'sum' should fold a list tail-recursively");
  __scheme_assert_eval_int(SUM "(sum nil 0)", 0,
                           "folding the empty list should give the initial accumulator");

  /* Build and consume in the same program: the list `upto` returns has to
     survive its own construction *and* the calls that walk it. */
  __scheme_assert_eval_int(
    "(define (upto n) (if (= n 0) nil (cons n (upto (- n 1)))))" SUM
    "(sum (upto 10) 0)", 55,
    "a constructed list should be foldable");
}

PRIVATE void __scheme_should_reject_bad_pair_operations(void)
{
  /* Taking the car of the empty list is the classic off-the-end bug, and it is
     an error in Scheme rather than a nil. */
  __scheme_assert_eval_status("(car nil)", ERR_SCHEME_NOT_A_PAIR,
                              "'car' of the empty list should be an error");
  __scheme_assert_eval_status("(cdr nil)", ERR_SCHEME_NOT_A_PAIR,
                              "'cdr' of the empty list should be an error");
  __scheme_assert_eval_status("(car 1)", ERR_SCHEME_NOT_A_PAIR,
                              "'car' of a number should be an error");
  __scheme_assert_eval_status("(cdr #t)", ERR_SCHEME_NOT_A_PAIR,
                              "'cdr' of a boolean should be an error");
  __scheme_assert_eval_status("(car (lambda (x) x))", ERR_SCHEME_NOT_A_PAIR,
                              "'car' of a procedure should be an error");
  __scheme_assert_eval_status("(cdr (cdr (cons 1 nil)))", ERR_SCHEME_NOT_A_PAIR,
                              "walking one cell past the end should be an error");

  __scheme_assert_eval_status("(cons 1)", ERR_SCHEME_BAD_ARITY,
                              "'cons' should require two arguments");
  __scheme_assert_eval_status("(cons)", ERR_SCHEME_BAD_ARITY,
                              "'cons' with no arguments should be an error");
  __scheme_assert_eval_status("(cons 1 2 3)", ERR_SCHEME_BAD_ARITY,
                              "'cons' should reject a third argument");
  __scheme_assert_eval_status("(car)", ERR_SCHEME_BAD_ARITY,
                              "'car' should require an argument");
  __scheme_assert_eval_status("(car (cons 1 2) 3)", ERR_SCHEME_BAD_ARITY,
                              "'car' should reject a second argument");
  __scheme_assert_eval_status("(null?)", ERR_SCHEME_BAD_ARITY,
                              "'null?' should require an argument");

  /* A pair is not a number, and arithmetic must say so rather than reading the
     cell pointer as an integer. */
  __scheme_assert_eval_status("(+ 1 (cons 1 2))", ERR_SCHEME_TYPE_ERROR,
                              "a pair should not be summable");
  __scheme_assert_eval_status("(< nil 1)", ERR_SCHEME_TYPE_ERROR,
                              "the empty list should not be comparable");

  // A pair is data, not a procedure.
  __scheme_assert_eval_status("((cons 1 2) 3)", ERR_SCHEME_NOT_APPLICABLE,
                              "a pair should not be applicable");
}

PRIVATE void __scheme_predicates_should_classify_values(void)
{
  __scheme_assert_eval_bool("(null? nil)", 1, "'null?' should be true of the empty list");
  __scheme_assert_eval_bool("(null? (cons 1 nil))", 0, "'null?' should be false of a pair");
  __scheme_assert_eval_bool("(null? 0)", 0, "'null?' should be false of zero");
  __scheme_assert_eval_bool("(null? #f)", 0, "'null?' should be false of false");

  __scheme_assert_eval_bool("(pair? (cons 1 2))", 1, "'pair?' should be true of a pair");
  __scheme_assert_eval_bool("(pair? nil)", 0, "'pair?' should be false of the empty list");
  __scheme_assert_eval_bool("(pair? 1)", 0, "'pair?' should be false of a number");
  __scheme_assert_eval_bool("(pair? car)", 0, "'pair?' should be false of a procedure");

  /* nil is not #f. Scheme has exactly one false value, and an empty list is not
     it — this is the difference from Lisp, and getting it wrong would silently
     change what every `if` over a list does. */
  __scheme_assert_eval_int("(if nil 1 2)", 1, "the empty list should be truthy");
  __scheme_assert_eval_int("(if (cons 1 2) 1 2)", 1, "a pair should be truthy");
}

/* nil is an ordinary global binding, not a keyword, so it shadows like one.
   This is a real difference from the special forms and worth stating. */
PRIVATE void __scheme_nil_should_be_an_ordinary_binding(void)
{
  __scheme_assert_eval_int("(define nil 5) nil", 5, "'nil' should be rebindable");
  __scheme_assert_eval_int("((lambda (nil) nil) 7)", 7, "'nil' should be usable as a parameter");
  __scheme_assert_eval_tag("((lambda (nil) 1) 7) nil", SCHEME_VALUE_TAG_NIL,
                           "shadowing 'nil' in a call should not disturb the global");
}

#define BUILD "(define (build n acc) (if (= n 0) acc (build (- n 1) (cons n acc))))"
#define LOOP  "(define (loop n acc) (if (= n 0) acc (loop (- n 1) (+ acc 1))))"

/* Pairs are heap objects; call frames are not. A tail loop that conses on every
   iteration therefore *must* grow, even though frame reuse means the loop
   itself allocates nothing — the frame is recycled, the cells it produced are
   not, because they are still reachable from the list being built.

   The two loops below are deliberately the same shape, same arity, same
   iteration count. The only difference is that one accumulates into a pair and
   the other into an integer, so the contrast isolates the cost of consing from
   everything else the program does. Measured minimum arenas at n=400: 8192 for
   LOOP, 32768 for BUILD. The collector cannot help here — every cell of the
   list `build` returns is live — so this gap is real work, not garbage.

   This is the allocation profile a collector exists to fix. Asserting it now
   means the eventual fix is measurable rather than assumed — when pairs start
   being reclaimed, the second half of this test is what will fail. */
PRIVATE void __scheme_conses_should_outlive_the_frame_that_made_them(void)
{
  __scheme_assert_eval_int(BUILD "(car (build 100 nil))", 1,
                           "a tail loop should cons a list that outlives its frames");
  __scheme_assert_eval_int(BUILD SUM "(sum (build 100 nil) 0)", 5050,
                           "every cell a tail loop conses should still be reachable");

  /* The control. Without it the NOMEM below would prove nothing: a budget too
     small for the *program* fails identically to one too small for its garbage,
     and 4096 is very nearly the floor for a two-parameter procedure. */
  __scheme_assert_eval_int_within(LOOP "(loop 400 0)", 8192, 400,
                                  "a non-consing tail loop should not grow with its iteration count");
  __scheme_assert_eval_status_within(BUILD "(build 400 nil)", 8192, ERR_SCHEME_NOMEM,
                                     "a loop retaining 400 cells should not fit in the same budget");
}

/* Allocates a pair on every iteration and immediately drops it: the `car` is an
   integer, so nothing refers to the cell once the iteration ends. This is pure
   garbage, and the amount of it is proportional to the iteration count, so the
   arena a program like this needs is a direct measure of whether anything is
   being reclaimed. */
#define TRASH "(define (trash n acc) (if (= n 0) acc (trash (- n 1) (car (cons n nil)))))"

PRIVATE void __scheme_assert_gc(const char *program, size_t arena_bytes, const scheme_config_t *config, int32_t expected_err, scheme_value_t *out, scheme_gc_stats_t *stats, const char *desc)
{
  arena_t arena = {0};
  arena_new_with_underlying_buffer(&arena, __buff, (ptrdiff_t)arena_bytes);

  scheme_value_t value = {0};
  scheme_gc_stats_t collected = {0};
  int32_t err = scheme_eval_with_config(&arena, string_view_from_cstr(program), config, &value, &collected);

  ASSERT_INT_EQ(expected_err, err, desc);

  if (out)   *out = value;
  if (stats) *stats = collected;
}

PRIVATE void __scheme_assert_gc_int(const char *program, size_t arena_bytes, const scheme_config_t *config, int32_t expected, const char *desc)
{
  scheme_value_t out = {0};
  __scheme_assert_gc(program, arena_bytes, config, 0, &out, NULL, desc);
  ASSERT_INT_EQ(SCHEME_VALUE_TAG_INT, out.tag, desc);
  ASSERT_INT_EQ(expected, out.as.val, desc);
}

/* The headline claim. Same program, same budget, same iteration count — the
   only difference is whether the collector is allowed to run. Measured floors:
   16KB with it, 512KB without, so the contrast is a factor of 32 and not
   sensitive to a few bytes moving either way. */
PRIVATE void __scheme_gc_should_reclaim_garbage(void)
{
  scheme_config_t on  = { .page_objects = 0, .gc_disabled = 0 };
  scheme_config_t off = { .page_objects = 0, .gc_disabled = 1 };

  __scheme_assert_gc_int(TRASH "(trash 10000 0)", 16384, &on, 1,
                         "a loop discarding a pair per iteration should run in a fixed arena");
  __scheme_assert_gc(TRASH "(trash 10000 0)", 16384, &off, ERR_SCHEME_NOMEM, NULL, NULL,
                     "the same loop should exhaust that arena with the collector disabled");

  /* Ten times the iterations, same budget. If anything at all leaked per
     iteration this would be the assertion that catches it. */
  __scheme_assert_gc_int(TRASH "(trash 100000 0)", 16384, &on, 1,
                         "ten times the garbage should still fit in the same arena");
}

PRIVATE void __scheme_gc_should_report_what_it_did(void)
{
  scheme_gc_stats_t stats = {0};
  scheme_config_t on = { .page_objects = 0, .gc_disabled = 0 };

  __scheme_assert_gc(TRASH "(trash 10000 0)", 16384, &on, 0, NULL, &stats,
                     "a garbage-heavy loop should evaluate");

  ASSERT_INT_EQ(1, stats.collections > 0, "a garbage-heavy loop should collect at least once");
  ASSERT_INT_EQ(1, stats.objects_allocated >= 10000,
                "every iteration should allocate a pair");
  ASSERT_INT_EQ(1, stats.objects_freed >= 9000,
                "nearly every allocated pair should be reclaimed");
  ASSERT_INT_EQ(1, stats.objects_live < 200,
                "almost nothing should be live at the end");
  ASSERT_INT_EQ(1, stats.pages == 1,
                "a fixed live set should never need a second page");

  /* The inverse: with the collector off, nothing is freed and the heap grows to
     the allocation count rather than the live set. */
  scheme_gc_stats_t without = {0};
  scheme_config_t off = { .page_objects = 0, .gc_disabled = 1 };
  __scheme_assert_gc(TRASH "(trash 10000 0)", BUFF_LEN, &off, 0, NULL, &without,
                     "the same loop should evaluate with the collector disabled");

  ASSERT_INT_EQ(0, (int32_t)without.collections, "a disabled collector should never run");
  ASSERT_INT_EQ(0, (int32_t)without.objects_freed, "a disabled collector should free nothing");
  ASSERT_INT_EQ(1, without.pages > 100, "a disabled collector should grow the heap instead");
}

/* Conservative stack scanning is what lets the evaluator keep values in
   ordinary C locals — `args[]`, `procedure`, `output` — without registering
   them anywhere. These programs put a freshly allocated object in exactly such
   a local and then run thousands of collections before using it again.

   The shape matters: operands are evaluated left to right into `args[]`, so
   `(cons X (trash ...))` evaluates X into args[0], and every collection the
   `trash` call triggers happens while args[0] is the only reference to it. If
   the scan missed that slot, the object would be swept and the value below
   would be wrong or the read would fault. */
PRIVATE void __scheme_gc_should_find_roots_on_the_c_stack(void)
{
  scheme_config_t on = { .page_objects = 0, .gc_disabled = 0 };

  __scheme_assert_gc_int(TRASH "(car (car (cons (cons 1 2) (trash 10000 0))))", 16384, &on, 1,
                         "a pair held in an operand slot should survive collections");
  __scheme_assert_gc_int(TRASH "(cdr (car (cons (cons 1 2) (trash 10000 0))))", 16384, &on, 2,
                         "both fields of a stack-held pair should survive");

  /* Same idea for a closure, which is the other collectable kind. It is applied
     *after* the collections, so it must still have its body and its env. */
  __scheme_assert_gc_int(TRASH "((car (cons (lambda (x) (* x 3)) (trash 10000 0))) 14)", 16384, &on, 42,
                         "a closure held in an operand slot should survive collections");

  /* A closure whose captured binding is itself a heap object: the env is a root,
     so the pair reached through it must be marked too. */
  __scheme_assert_gc_int(
    TRASH
    "(define (hold p) (lambda () (car p)))"
    "((car (cons (hold (cons 7 nil)) (trash 10000 0))))", 16384, &on, 7,
    "a pair reachable only through a closure's environment should survive");
}

/* The other half of correctness, and the easier one to get wrong in the
   direction of a silent wrong answer: a collector that frees too much. The list
   here is entirely live, so a correct sweep frees none of it. */
PRIVATE void __scheme_gc_should_not_reclaim_live_data(void)
{
  /* A small page forces the free list to empty often, so a 500-cell list is
     built across many collections rather than one. Every one of them sees the
     partially built list held only through `acc` in a call frame. */
  scheme_config_t tiny = { .page_objects = 8, .gc_disabled = 0 };

  __scheme_assert_gc_int(BUILD SUM "(sum (build 500 nil) 0)", BUFF_LEN, &tiny, 125250,
                         "a list built across many collections should be intact");
  /* 200, not 500: `length` recurses non-tail, so a longer list would hit
     __SCHEME_MAX_DEPTH before it could say anything about the collector. */
  __scheme_assert_gc_int(BUILD LENGTH "(length (build 200 nil))", BUFF_LEN, &tiny, 200,
                         "no cell of a live list should be reclaimed");

  scheme_gc_stats_t stats = {0};
  __scheme_assert_gc(BUILD "(build 500 nil)", BUFF_LEN, &tiny, 0, NULL, &stats,
                     "a wholly live list should evaluate");
  ASSERT_INT_EQ(0, (int32_t)stats.objects_freed,
                "a program whose every cell is live should free nothing");
  ASSERT_INT_EQ(1, stats.objects_live >= 500,
                "every cell of a live list should still be live at the end");

  /* Interleaved: garbage and live data allocated in the same loop. The
     collector has to tell them apart rather than keeping or dropping wholesale. */
  __scheme_assert_gc_int(
    "(define (mixed n acc) (if (= n 0) acc (mixed (- n 1) (cons (car (cons n nil)) acc))))"
    SUM "(sum (mixed 500 nil) 0)", BUFF_LEN, &tiny, 125250,
    "garbage allocated alongside live cells should not take them with it");
}

/* Page size is policy: it trades collection frequency against footprint, and
   the caller sets it. Nothing about the answers may change. */
PRIVATE void __scheme_gc_should_honour_its_configuration(void)
{
  scheme_gc_stats_t small = {0};
  scheme_gc_stats_t large = {0};
  scheme_config_t small_pages = { .page_objects = 16,  .gc_disabled = 0 };
  scheme_config_t large_pages = { .page_objects = 256, .gc_disabled = 0 };

  __scheme_assert_gc(TRASH "(trash 20000 0)", BUFF_LEN, &small_pages, 0, NULL, &small,
                     "a small page should still evaluate the program");
  __scheme_assert_gc(TRASH "(trash 20000 0)", BUFF_LEN, &large_pages, 0, NULL, &large,
                     "a large page should still evaluate the program");

  ASSERT_INT_EQ(1, small.collections > large.collections,
                "a smaller page should collect more often");
  ASSERT_INT_EQ(1, small.pages == 1, "a fixed live set should fit one small page");
  ASSERT_INT_EQ(1, large.pages == 1, "a fixed live set should fit one large page");

  // Same program, same answer, whatever the policy.
  __scheme_assert_gc_int(TRASH "(trash 20000 0)", BUFF_LEN, &small_pages, 1,
                         "a small page should not change the answer");
  __scheme_assert_gc_int(TRASH "(trash 20000 0)", BUFF_LEN, &large_pages, 1,
                         "a large page should not change the answer");

  // A NULL config is the documented way to ask for the defaults.
  __scheme_assert_gc_int(TRASH "(trash 20000 0)", BUFF_LEN, NULL, 1,
                         "a NULL config should select the defaults");
}

/* A slot handed back out must not carry anything from the object that used to
   be there. A stale field would be traced on the next collection and keep real
   garbage alive — a leak that only shows up under reuse, which is exactly the
   case these loops produce. */
PRIVATE void __scheme_gc_should_not_resurrect_recycled_slots(void)
{
  scheme_config_t tiny = { .page_objects = 8, .gc_disabled = 0 };
  scheme_gc_stats_t stats = {0};

  /* Every iteration builds a two-cell list and drops it, so slots are recycled
     continuously and each new cell lands where a dead one was. */
  __scheme_assert_gc(
    "(define (spin n acc) (if (= n 0) acc (spin (- n 1) (car (cons n (cons n nil))))))"
    "(spin 5000 0)", 16384, &tiny, 0, NULL, &stats,
    "a loop recycling slots should evaluate");

  ASSERT_INT_EQ(1, stats.objects_live < 100,
                "recycled slots should not retain the objects that preceded them");
  ASSERT_INT_EQ(1, stats.pages == 1,
                "a loop with a fixed live set should never grow past one page");
}

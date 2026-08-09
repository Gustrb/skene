#include "libstrview/string_view.h"
#include <libhashtable/swisstables.h>
#include <libscheme/scheme.h>

#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/asan_interface.h>

/* AddressSanitizer's "fake stack" and conservative root scanning cannot coexist.
   With detect_stack_use_after_return — on by default since GCC 12 — a function's
   locals are moved off the machine stack into a heap-allocated frame, so
   scanning between two stack addresses finds a *pointer to* the frame instead of
   the values inside it. The collector then sees no roots, frees live objects,
   and the program quietly computes the wrong answer. That is precisely what it
   did before this hook existed: `(car (car (cons (cons 1 2) <allocates>)))`
   returned 39 instead of 1.

   ASan reads this hook at startup, so any program linking libscheme gets the
   setting without needing a build flag or an environment variable. The check in
   __scheme_gc_collect covers the case where it is overridden anyway. */
const char *__asan_default_options(void);
const char *__asan_default_options(void)
{
  return "detect_stack_use_after_return=0";
}
#endif

typedef struct {
  string_view_t input;
  size_t position;
  size_t read_position;
  char ch;

  size_t line;
  size_t column;
} __scheme_lexer_t;

typedef enum {
  __SCHEME_TOKEN_TYPE_NONE = 0,
  __SCHEME_TOKEN_TYPE_NUMBER,
  __SCHEME_TOKEN_TYPE_BOOLEAN,
  __SCHEME_TOKEN_TYPE_LPAREN,
  __SCHEME_TOKEN_TYPE_RPAREN,
  __SCHEME_TOKEN_TYPE_SYMBOL,
  __SCHEME_TOKEN_TYPE_EOF,
} __scheme_token_type_t;

typedef struct {
  string_view_t value;
  __scheme_token_type_t type;

  size_t line;
  size_t column;
} __scheme_token_t;

typedef struct {
  arena_t *arena;
  __scheme_lexer_t lexer;

  __scheme_token_t curr_token;
  __scheme_token_t peek_token;
} __scheme_parser_t;

PRIVATE uint8_t __is_digit(char c);
PRIVATE uint8_t __is_delimiter(char c);
PRIVATE void __scheme_lexer_new(__scheme_lexer_t *lexer, string_view_t file);
PRIVATE void __scheme_lexer_read_char(__scheme_lexer_t *lexer);
PRIVATE int32_t __scheme_lexer_next_token(__scheme_lexer_t *lexer, __scheme_token_t *tok);
PRIVATE void __scheme_lexer_skip_whitespace(__scheme_lexer_t *lexer);
PRIVATE char __scheme_lexer_peek_char(__scheme_lexer_t *lexer);
PRIVATE void __scheme_lexer_read_num(__scheme_lexer_t *lexer, string_view_t *);
PRIVATE void __scheme_lexer_read_symbol(__scheme_lexer_t *lexer, string_view_t *);

typedef enum {
  __SCHEME_EXPRESSION_TYPE_NUMBER,
  __SCHEME_EXPRESSION_TYPE_BOOLEAN,
  __SCHEME_EXPRESSION_TYPE_SYMBOL,
  __SCHEME_EXPRESSION_TYPE_LIST,
  __SCHEME_EXPRESSION_TYPE_NIL,
} __scheme_expression_type_t;

typedef struct __scheme_expression_t __scheme_expression_t;

typedef struct {
  __scheme_expression_t *car;
  __scheme_expression_t *cdr;
} __scheme_cons_t;

typedef struct __scheme_expression_t {
  __scheme_expression_type_t type;

  union {
    int32_t num;
    uint8_t boolean;
    // Borrows the caller's input buffer, not the arena.
    string_view_t sym;
    __scheme_cons_t list;
  } as;
} __scheme_expression_t;

// The single shared list terminator. Nil is immutable, so every proper list
// ends at this instance; nothing may ever write through it.
PRIVATE __scheme_expression_t __scheme_nil = {
  .type = __SCHEME_EXPRESSION_TYPE_NIL,
  .as = {0},
};

/* Every collectable object starts with one of these, so a slot can be
   classified without knowing what is in it. `kind` doubles as the liveness
   flag: page memory arrives zeroed from the arena, and zero is FREE, so a
   never-allocated slot classifies correctly with no extra initialisation. */
typedef enum {
  __SCHEME_OBJ_FREE = 0,
  __SCHEME_OBJ_PAIR,
  __SCHEME_OBJ_CLOSURE,
} __scheme_obj_kind_t;

typedef struct {
  uint8_t kind;
  uint8_t mark;
  /* Only meaningful while the object is on the free list. Overlapping it with
     live object data would save eight bytes per slot and cost the clarity of
     being able to look at any slot at any time and see what it is. */
  void *next_free;
} __scheme_obj_header_t;

/* A cons cell. Both fields are full values rather than pointers, so a pair
   holding two integers costs one allocation, not three. */
struct scheme_pair_t {
  __scheme_obj_header_t header;
  scheme_value_t car;
  scheme_value_t cdr;
};

typedef struct __scheme_env_t __scheme_env_t;

/* A scope. Bindings map a symbol name to an arena-allocated scheme_value_t;
   sw_table_t borrows both, so the key's bytes must outlive the env. For the
   global scope those are string literals, and for user bindings they point into
   the caller's input buffer. `parent` is NULL for the global scope.

   Envs are themselves arena-allocated rather than living on the C stack: a
   closure captures the env pointer it was created in, and that pointer has to
   stay valid for as long as the closure does, which outlasts the evaluator
   frame that created it. */
struct __scheme_env_t {
  __scheme_env_t *parent;
  // The heap every allocation below this env draws from, carried here so the
  // evaluator can define bindings without threading it through every call.
  scheme_heap_t *heap;
  /* Every env ever created, in one chain. Environments are *not* collected —
     see the note on the heap below — so this is not a free list but a trace
     list: the collector walks it to find the values the bindings hold. */
  __scheme_env_t *next_env;
  sw_table_t bindings;
  /* Set once a closure has closed over this scope, or over any scope beneath
     it. A captured frame may outlive the call that created it, so its storage
     can never be handed to a later call — see __scheme_apply_closure. The flag
     is sticky and conservative: it is never cleared, so a frame whose capturing
     closure has itself become garbage stays un-reusable until there is a real
     collector to say otherwise. */
  uint8_t captured;
};

#define __SCHEME_ENV_INITIAL_CAP 16
/* A call frame only ever holds the callee's parameters, of which there are at
   most __SCHEME_MAX_ARGS. Sizing it below the global's capacity keeps the
   per-call arena cost down: nothing is ever reclaimed, so a recursive call
   chain pays this for every activation. */
#define __SCHEME_CALL_ENV_CAP 8
// Cap on arguments to a single application, so evaluated arguments can live in
// a stack array instead of needing a second arena pass.
#define __SCHEME_MAX_ARGS 16
/* Ceiling on evaluator recursion. Non-tail subexpressions cost a real C frame
   each, so without this a program like `(define (f n) (+ 1 (f n))) (f 0)` would
   run until the stack faults. Tail calls do not count against it — they run on
   the trampoline's own frame — so `(define (loop) (loop))` is not bounded by
   this and never will be; it is an infinite loop, exactly as in any Scheme. */
#define __SCHEME_MAX_DEPTH 256

/* A user procedure. `params` is the cons chain of parameter symbols and `body`
   the cons chain of body expressions; both point into the parsed program, which
   lives in the same arena. `env` is the scope the lambda was written in, which
   is what makes this a closure rather than a plain function. */
struct scheme_closure_t {
  __scheme_obj_header_t header;
  __scheme_expression_t *params;
  size_t arity;
  __scheme_expression_t *body;
  __scheme_env_t *env;
};

/* One size class for the whole heap. A pair and a closure are within a few
   bytes of each other, so giving every object the larger of the two wastes
   almost nothing and buys a great deal: one free list instead of several, and
   an O(1) "is this address an object start?" test, which is what makes the
   conservative stack scan below cheap enough to be worth doing. */
#define __SCHEME_SLOT_ALIGN 16
#define __SCHEME_SLOT_RAW \
  (sizeof(scheme_pair_t) > sizeof(scheme_closure_t) ? sizeof(scheme_pair_t) : sizeof(scheme_closure_t))
#define __SCHEME_SLOT_SIZE \
  (((__SCHEME_SLOT_RAW) + __SCHEME_SLOT_ALIGN - 1) / __SCHEME_SLOT_ALIGN * __SCHEME_SLOT_ALIGN)

#define __SCHEME_DEFAULT_PAGE_OBJECTS 64

/* A run of slots, bump-allocated from the arena once and never given back. The
   arena cannot free, so pages are the granularity at which the heap grows and
   the collector recycles *within* them rather than returning anything. */
typedef struct __scheme_page_t __scheme_page_t;
struct __scheme_page_t {
  __scheme_page_t *next;
  char *slots;
  size_t slot_count;
};

/* The runtime's allocator: a mark-sweep collector over a free list of
   fixed-size slots, carved from arena-backed pages.

   Roots come from three places. The C stack is scanned conservatively — every
   aligned word between the collector's own frame and the base recorded at
   entry is tested for being an object address — which is what lets the
   evaluator hold values in ordinary locals like `args[]` without registering
   anything. `result_slot` covers the one live value the scan cannot see,
   because it lives in the *caller's* frame, above the recorded base. And
   `envs` is walked for the values held in bindings.

   Environments are not collected. They are arena-allocated and permanently
   live, so every env ever created is traced, and anything its bindings reach
   stays reachable. For a tail loop that recycles one frame this is exact: the
   bindings are overwritten, so what they used to hold becomes garbage on
   schedule. For a loop that captures a frame per iteration it is not — those
   envs accumulate and pin everything they hold. Collecting them needs the
   binding table to be freeable, which needs libhashtable to allocate from
   something other than an arena; that is the next piece of work, not this
   one. */
struct scheme_heap_t {
  arena_t *arena;

  __scheme_page_t *pages;
  void *free_list;
  size_t page_objects;
  /* Live objects that must be reached before a collection is worth attempting;
     recomputed from the live set after each one. See __scheme_heap_alloc. */
  size_t gc_threshold;
  uint8_t gc_disabled;

  __scheme_env_t *envs;
  scheme_value_t *result_slot;
  /* Address of a local in scheme_eval's frame: the high end of the region the
     conservative scan covers. The stack grows down on the only platform
     libarena supports, so every evaluator frame sits below this. */
  const char *stack_base;

  scheme_gc_stats_t stats;
};

PRIVATE int32_t __scheme_env_new(__scheme_env_t *env, scheme_heap_t *heap, __scheme_env_t *parent, size_t cap);
PRIVATE int32_t __scheme_env_define(__scheme_env_t *env, string_view_t name, scheme_value_t value);
PRIVATE int32_t __scheme_env_lookup(__scheme_env_t *env, string_view_t name, scheme_value_t *out);
PRIVATE int32_t __scheme_env_install_primitives(__scheme_env_t *env);

PRIVATE void *__scheme_heap_alloc(scheme_heap_t *heap, __scheme_obj_kind_t kind);
PRIVATE int32_t __scheme_heap_add_page(scheme_heap_t *heap);
PRIVATE void __scheme_gc_collect(scheme_heap_t *heap);
PRIVATE void __scheme_gc_mark_roots(scheme_heap_t *heap);
PRIVATE void __scheme_gc_mark_stack(scheme_heap_t *heap, const char *low, const char *high);
PRIVATE void __scheme_gc_mark_address(scheme_heap_t *heap, const void *addr);
PRIVATE void __scheme_gc_mark_value(scheme_heap_t *heap, scheme_value_t value);
PRIVATE void __scheme_gc_sweep(scheme_heap_t *heap);

PRIVATE int32_t __scheme_prim_add(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_sub(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_mul(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_num_eq(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_lt(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_gt(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_le(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_ge(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);

PRIVATE int32_t __scheme_prim_cons(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_car(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_cdr(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_is_null(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);
PRIVATE int32_t __scheme_prim_is_pair(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);

PRIVATE int32_t __scheme_parser_new(__scheme_parser_t *parser, __scheme_lexer_t lexer, arena_t *arena);
PRIVATE int32_t __scheme_parser_advance(__scheme_parser_t *parser);
PRIVATE int32_t __scheme_parser_parse_expression(__scheme_parser_t *, __scheme_expression_t **out);
PRIVATE int32_t __scheme_parser_parse_list(__scheme_parser_t *, __scheme_expression_t **out);
PRIVATE int32_t __scheme_parser_alloc_expression(__scheme_parser_t *, __scheme_expression_t **out);
PRIVATE uint8_t __scheme_parser_is_done(__scheme_parser_t *p);

/* Work handed back to the trampoline in __scheme_eval_expression instead of
   being evaluated on the spot. An expression in tail position is the whole
   remaining value of the call that produced it, so rather than recursing into
   it — and holding a C frame that has nothing left to do — the callee parks it
   here and the trampoline continues with it in place of what it was evaluating.

   `expr == NULL` means "no tail work, the value is already in `output`". */
typedef struct {
  __scheme_expression_t *expr;
  __scheme_env_t *env;
} __scheme_tail_t;

/* `depth` counts *non-tail* nesting only, and is checked at the top of the
   trampoline. A tail call replaces the current evaluation rather than nesting
   inside it, so it iterates at the same depth and can run forever; that is what
   makes a loop written as tail recursion a loop rather than a stack overflow.
   `depth` is passed by value, so it unwinds on its own with no counter to
   decrement. Anything that evaluates a *sub*expression passes `depth + 1`. */
PRIVATE int32_t __scheme_eval_expression(__scheme_expression_t *expr, __scheme_env_t *env, size_t depth, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_list_expression(__scheme_cons_t list, __scheme_env_t *env, size_t depth, __scheme_env_t *reusable, __scheme_tail_t *tail, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_nil_expression(scheme_value_t *output);
PRIVATE int32_t __scheme_eval_symbol_expression(string_view_t symbol, __scheme_env_t *env, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_number_expression(int32_t number, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_boolean_expression(uint8_t boolean, scheme_value_t *output);

/* Special forms. Each receives the *unevaluated* operand chain — that is the
   whole point of being a special form — plus the env and depth it was reached
   at. Those with a tail position also receive `tail` to park it in. */
PRIVATE int32_t __scheme_eval_define(__scheme_expression_t *operands, __scheme_env_t *env, size_t depth, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_lambda(__scheme_expression_t *operands, __scheme_env_t *env, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_if(__scheme_expression_t *operands, __scheme_env_t *env, size_t depth, __scheme_tail_t *tail, scheme_value_t *output);

PRIVATE int32_t __scheme_make_closure(__scheme_expression_t *params, __scheme_expression_t *body, __scheme_env_t *env, scheme_value_t *output);
PRIVATE int32_t __scheme_apply_closure(scheme_closure_t *closure, const scheme_value_t *args, size_t argc, size_t depth, __scheme_env_t *reusable, __scheme_tail_t *tail);
PRIVATE void __scheme_env_mark_captured(__scheme_env_t *env);
PRIVATE uint8_t __scheme_frame_fits_params(__scheme_env_t *frame, const scheme_closure_t *closure);
PRIVATE uint8_t __scheme_is_truthy(scheme_value_t value);
PRIVATE size_t __scheme_list_length(__scheme_expression_t *list);
PRIVATE __scheme_expression_t *__scheme_list_nth(__scheme_expression_t *list, size_t n);

/*
  PROGRAM    = EXPRESSION*
  EXPRESSION = NUMBER | BOOLEAN | SYMBOL | LIST
  LIST       = '(' EXPRESSION* ')'

  Three list shapes are special forms rather than applications, recognised by
  their head symbol before anything is evaluated:

  DEFINE     = '(' 'define' SYMBOL EXPRESSION ')'
             | '(' 'define' '(' SYMBOL SYMBOL* ')' EXPRESSION+ ')'
  LAMBDA     = '(' 'lambda' '(' SYMBOL* ')' EXPRESSION+ ')'
  IF         = '(' 'if' EXPRESSION EXPRESSION EXPRESSION? ')'
*/

int32_t scheme_eval(arena_t *arena, string_view_t input, scheme_value_t *output)
{
  return scheme_eval_with_config(arena, input, NULL, output, NULL);
}

int32_t scheme_eval_with_config(arena_t *arena, string_view_t input, const scheme_config_t *config, scheme_value_t *output, scheme_gc_stats_t *stats)
{
  /* Anchors the conservative root scan. Every frame the evaluator makes sits
     below this address, so [collector frame, here) covers all of them — and
     nothing above it is ours to interpret. It must be a local of *this*
     function: taking it any deeper would leave the frames above it unscanned,
     and the values they hold would be collected while still in use. */
  const char stack_base = 0;

  __scheme_lexer_t lexer = {0};
  __scheme_lexer_new(&lexer, input);

  __scheme_parser_t parser = {0};
  TRY(__scheme_parser_new(&parser, lexer, arena));

  /* The heap and the global env are arena-allocated, not locals: a closure
     defined at top level captures the env pointer — which in turn holds the
     heap — and may be handed back through `output`, which outlives this
     frame. */
  scheme_heap_t *heap = arena_new(arena, scheme_heap_t);
  if (heap == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  heap->arena = arena;
  heap->page_objects = __SCHEME_DEFAULT_PAGE_OBJECTS;
  heap->stack_base = &stack_base;
  heap->result_slot = output;

  if (config != NULL)
  {
    if (config->page_objects != 0)
    {
      heap->page_objects = config->page_objects;
    }
    heap->gc_disabled = config->gc_disabled;
  }

  heap->gc_threshold = heap->page_objects;

  __scheme_env_t *global = arena_new(arena, __scheme_env_t);
  if (global == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  TRY(__scheme_env_new(global, heap, NULL, __SCHEME_ENV_INITIAL_CAP));
  TRY(__scheme_env_install_primitives(global));

  __scheme_expression_t *expr = NULL;
  int32_t err = 0;
  while (!__scheme_parser_is_done(&parser))
  {
    err = __scheme_parser_parse_expression(&parser, &expr);
    if (err != 0) break;

    err = __scheme_eval_expression(expr, global, 0, output);
    if (err != 0) break;
  }

  /* Reported even on failure: how much was allocated before a program ran out
     of memory is exactly what a caller investigating ERR_SCHEME_NOMEM wants. */
  if (stats != NULL)
  {
    *stats = heap->stats;
  }

  return err;
}

/* The trampoline. Everything that is *not* in tail position recurses into this
   function normally and costs a C frame; everything that is comes back through
   `tail` and is picked up by the next turn of this loop, at the same depth and
   on the same C frame. Tail-recursive Scheme therefore runs in constant stack,
   and __SCHEME_MAX_DEPTH bounds only genuine nesting.

   Note that the compiler cannot be relied on to do this for us. C guarantees
   nothing about sibling-call optimisation: whether `return __scheme_eval_
   expression(...)` becomes a jump or a call depends on the optimisation level,
   and sanitizer instrumentation inhibits it besides. Proper tail calls are a
   semantic promise of the language being implemented, so the interpreter has to
   make them itself. */
PRIVATE int32_t __scheme_eval_expression(__scheme_expression_t *expr, __scheme_env_t *env, size_t depth, scheme_value_t *output)
{
  /* A call frame that this loop allocated and that nothing outside it can
     reach, so the next tail call may take its storage rather than bumping a new
     one out of the arena. It starts NULL on purpose: the `env` we were handed
     belongs to whoever called us, who may still be using it. Only frames we
     made ourselves are ours to recycle. */
  __scheme_env_t *reusable = NULL;

  for (;;)
  {
    if (depth >= __SCHEME_MAX_DEPTH)
    {
      return ERR_SCHEME_DEPTH_EXCEEDED;
    }

    __scheme_tail_t tail = {0};

    switch (expr->type)
    {
    case __SCHEME_EXPRESSION_TYPE_LIST:
    {
      TRY(__scheme_eval_list_expression(expr->as.list, env, depth, reusable, &tail, output));
    }; break;
    case __SCHEME_EXPRESSION_TYPE_NIL:
    {
      TRY(__scheme_eval_nil_expression(output));
    }; break;
    case __SCHEME_EXPRESSION_TYPE_NUMBER:
    {
      TRY(__scheme_eval_number_expression(expr->as.num, output));
    }; break;
    case __SCHEME_EXPRESSION_TYPE_BOOLEAN:
    {
      TRY(__scheme_eval_boolean_expression(expr->as.boolean, output));
    }; break;
    case __SCHEME_EXPRESSION_TYPE_SYMBOL:
    {
      TRY(__scheme_eval_symbol_expression(expr->as.sym, env, output));
    }; break;
    }

    // Only a list expression ever parks tail work; everything else is already
    // a value.
    if (tail.expr == NULL)
    {
      return 0;
    }

    /* A changed env means a call happened and `tail.env` is the frame it made
       or recycled; either way it is ours from here on. An unchanged env means
       an `if` picked a branch in the scope we were already in, which leaves the
       reuse candidate exactly as it was. */
    if (tail.env != env)
    {
      reusable = tail.env;
    }

    expr = tail.expr;
    env = tail.env;
  }
}

/* Procedure application, and the special forms that must pre-empt it.

   A special form is recognised by its head symbol *before* anything is
   evaluated, because the normal rule below — evaluate the head, then every
   operand — is exactly what they cannot tolerate: `define` needs its name
   unevaluated, `lambda` needs its body unevaluated, and `if` must evaluate only
   the branch it selects.

   The names are keywords, not bindings: they are matched textually and never
   looked up, so `(define if 3)` binds a variable that `(if ...)` will not see.
   That is the usual trade for a Scheme without a macro expander. */
PRIVATE int32_t __scheme_eval_list_expression(__scheme_cons_t list, __scheme_env_t *env, size_t depth, __scheme_env_t *reusable, __scheme_tail_t *tail, scheme_value_t *output)
{
  if (list.car->type == __SCHEME_EXPRESSION_TYPE_SYMBOL)
  {
    string_view_t head = list.car->as.sym;
    if (string_view_equals(head, string_view_from_cstr("define")))
    {
      return __scheme_eval_define(list.cdr, env, depth, output);
    }
    if (string_view_equals(head, string_view_from_cstr("lambda")))
    {
      return __scheme_eval_lambda(list.cdr, env, output);
    }
    if (string_view_equals(head, string_view_from_cstr("if")))
    {
      return __scheme_eval_if(list.cdr, env, depth, tail, output);
    }
  }

  /* The operator and operands are subexpressions of this application, not
     replacements for it, so they nest: depth + 1, and a real C frame each. */
  scheme_value_t procedure = {0};
  TRY(__scheme_eval_expression(list.car, env, depth + 1, &procedure));

  if (procedure.tag != SCHEME_VALUE_TAG_PRIMITIVE
      && procedure.tag != SCHEME_VALUE_TAG_CLOSURE)
  {
    return ERR_SCHEME_NOT_APPLICABLE;
  }

  scheme_value_t args[__SCHEME_MAX_ARGS] = {0};
  size_t argc = 0;

  for (__scheme_expression_t *p = list.cdr;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr)
  {
    if (argc >= __SCHEME_MAX_ARGS)
    {
      return ERR_SCHEME_TOO_MANY_ARGS;
    }

    TRY(__scheme_eval_expression(p->as.list.car, env, depth + 1, &args[argc]));
    argc++;
  }

  /* A primitive returns a value directly — there is no Scheme expression left
     to evaluate, so nothing can be in tail position. A closure body is, which
     is why applying one parks work instead of recursing. */
  if (procedure.tag == SCHEME_VALUE_TAG_CLOSURE)
  {
    return __scheme_apply_closure(procedure.as.closure, args, argc, depth, reusable, tail);
  }

  return procedure.as.prim(env->heap, args, argc, output);
}

/* `(define <name> <expr>)` binds the value of <expr>; `(define (<name> <param>...)
   <body>...)` is sugar for binding a lambda. Both evaluate to the bound value,
   which R7RS leaves unspecified but is the most useful thing to hand back.

   The binding is installed in the *same* env the closure captured, so a
   procedure defined this way can see its own name and recurse. */
PRIVATE int32_t __scheme_eval_define(__scheme_expression_t *operands, __scheme_env_t *env, size_t depth, scheme_value_t *output)
{
  if (operands->type != __SCHEME_EXPRESSION_TYPE_LIST)
  {
    return ERR_SCHEME_BAD_SYNTAX;
  }

  __scheme_expression_t *target = operands->as.list.car;
  __scheme_expression_t *rest = operands->as.list.cdr;

  if (target->type == __SCHEME_EXPRESSION_TYPE_SYMBOL)
  {
    if (__scheme_list_length(rest) != 1)
    {
      return ERR_SCHEME_BAD_SYNTAX;
    }

    /* Not a tail position: the value is consumed by the binding, so `define`
       has work left to do after it and must nest. */
    scheme_value_t value = {0};
    TRY(__scheme_eval_expression(rest->as.list.car, env, depth + 1, &value));
    TRY(__scheme_env_define(env, target->as.sym, value));

    *output = value;
    return 0;
  }

  if (target->type == __SCHEME_EXPRESSION_TYPE_LIST)
  {
    __scheme_expression_t *name = target->as.list.car;
    if (name->type != __SCHEME_EXPRESSION_TYPE_SYMBOL)
    {
      return ERR_SCHEME_BAD_SYNTAX;
    }

    scheme_value_t closure = {0};
    TRY(__scheme_make_closure(target->as.list.cdr, rest, env, &closure));
    TRY(__scheme_env_define(env, name->as.sym, closure));

    *output = closure;
    return 0;
  }

  // `(define () ...)` and `(define 1 ...)` land here.
  return ERR_SCHEME_BAD_SYNTAX;
}

// Nothing is evaluated: the operands are packaged verbatim into a closure.
PRIVATE int32_t __scheme_eval_lambda(__scheme_expression_t *operands, __scheme_env_t *env, scheme_value_t *output)
{
  if (operands->type != __SCHEME_EXPRESSION_TYPE_LIST)
  {
    return ERR_SCHEME_BAD_SYNTAX;
  }

  return __scheme_make_closure(operands->as.list.car, operands->as.list.cdr, env, output);
}

/* Only the selected branch is evaluated, which is what makes `if` a special
   form rather than a procedure: a procedure receives values, and computing both
   branches to produce them defeats the purpose. */
PRIVATE int32_t __scheme_eval_if(__scheme_expression_t *operands, __scheme_env_t *env, size_t depth, __scheme_tail_t *tail, scheme_value_t *output)
{
  size_t n = __scheme_list_length(operands);
  if (n < 2 || n > 3)
  {
    return ERR_SCHEME_BAD_SYNTAX;
  }

  // The test is a subexpression — its value is consumed here — so it nests.
  scheme_value_t test = {0};
  TRY(__scheme_eval_expression(__scheme_list_nth(operands, 0), env, depth + 1, &test));

  /* The selected branch is not consumed here: its value *is* the value of the
     `if`, so it goes back to the trampoline rather than being evaluated in a
     nested frame. This is what puts the recursive call of
     `(if (= n 0) acc (loop (- n 1) ...))` in tail position. */
  if (__scheme_is_truthy(test))
  {
    tail->expr = __scheme_list_nth(operands, 1);
    tail->env = env;
    return 0;
  }

  if (n == 3)
  {
    tail->expr = __scheme_list_nth(operands, 2);
    tail->env = env;
    return 0;
  }

  // A one-armed `if` that fails its test has no value in R7RS; #f is the least
  // surprising thing to produce and keeps every expression total.
  output->tag = SCHEME_VALUE_TAG_BOOL;
  output->as.boolean = 0;
  return 0;
}

/* `params` must be a (possibly empty) list of symbols and `body` at least one
   expression. Both are borrowed from the parsed program rather than copied,
   which is safe because the AST and the closure share the caller's arena. */
PRIVATE int32_t __scheme_make_closure(__scheme_expression_t *params, __scheme_expression_t *body, __scheme_env_t *env, scheme_value_t *output)
{
  if (params->type != __SCHEME_EXPRESSION_TYPE_LIST
      && params->type != __SCHEME_EXPRESSION_TYPE_NIL)
  {
    return ERR_SCHEME_BAD_SYNTAX;
  }

  size_t arity = 0;
  for (__scheme_expression_t *p = params;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr)
  {
    if (p->as.list.car->type != __SCHEME_EXPRESSION_TYPE_SYMBOL)
    {
      return ERR_SCHEME_BAD_SYNTAX;
    }
    arity++;
  }

  // Arguments are collected into a fixed stack array at the call site, so a
  // procedure that could never be called is rejected at construction instead.
  if (arity > __SCHEME_MAX_ARGS)
  {
    return ERR_SCHEME_TOO_MANY_ARGS;
  }

  if (body->type != __SCHEME_EXPRESSION_TYPE_LIST)
  {
    return ERR_SCHEME_BAD_SYNTAX;
  }

  scheme_closure_t *closure = __scheme_heap_alloc(env->heap, __SCHEME_OBJ_CLOSURE);
  if (closure == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  closure->params = params;
  closure->arity = arity;
  closure->body = body;
  closure->env = env;

  /* This closure can outlive the call that produced it, and it holds `env`, so
     from here on no call frame in that chain may be recycled underneath it. */
  __scheme_env_mark_captured(env);

  output->tag = SCHEME_VALUE_TAG_CLOSURE;
  output->as.closure = closure;
  return 0;
}

/* Marking walks up the parent chain and stops at the first scope already
   flagged: whoever flagged it walked the rest of the chain at the time, and a
   frame is only ever given a new parent while it is *un*flagged — that is the
   reuse path in __scheme_apply_closure — so everything above a flagged scope is
   flagged too. */
PRIVATE void __scheme_env_mark_captured(__scheme_env_t *env)
{
  for (__scheme_env_t *scope = env; scope != NULL && !scope->captured; scope = scope->parent)
  {
    scope->captured = 1;
  }
}

/* True when rebinding `closure`'s parameters into `frame` would leave the frame
   holding exactly those names and nothing else — in which case the frame needs
   no clearing and no fresh boxes.

   The binding count has to match the arity as well as every name being present:
   a body `define` may have added a name that is not a parameter, and that name
   would go on shadowing an outer binding for the next call. Duplicate
   parameters fail the count check too, which is the conservative answer. */
PRIVATE uint8_t __scheme_frame_fits_params(__scheme_env_t *frame, const scheme_closure_t *closure)
{
  if ((size_t)frame->bindings.resident != closure->arity)
  {
    return 0;
  }

  for (__scheme_expression_t *p = closure->params;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr)
  {
    void *found = NULL;
    if (sw_table_find(&frame->bindings, p->as.list.car->as.sym, &found) != 1)
    {
      return 0;
    }
  }

  return 1;
}

/* Binds the arguments in a fresh frame whose parent is the *captured* env, not
   the caller's — that is what gives lexical rather than dynamic scope.

   The body is a sequence: every expression but the last runs for its effect
   here, and the last is parked as tail work. Nothing about this call remains to
   be done once that last expression starts, so there is no reason to hold a C
   frame for it — which is exactly the definition of a tail call. */
PRIVATE int32_t __scheme_apply_closure(scheme_closure_t *closure, const scheme_value_t *args, size_t argc, size_t depth, __scheme_env_t *reusable, __scheme_tail_t *tail)
{
  if (argc != closure->arity)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  scheme_heap_t *heap = closure->env->heap;
  __scheme_env_t *frame = NULL;

  if (reusable != NULL && !reusable->captured)
  {
    /* Recycle the frame the trampoline is about to walk away from. Every
       operand was evaluated before this function was reached, so there is
       nothing left to read out of it, and `captured` being clear means no
       closure kept a reference. Dropping the bindings and re-pointing the
       parent at the callee's scope turns the whole thing into the same storage
       under a new name — which is what lets a tail loop run without touching
       the arena at all after its first iteration.

       `closure->env` cannot be this frame or anything below it: a closure
       written there would have marked this frame captured on the way out. */
    frame = reusable;

    /* Clearing is what makes the frame safe for a *different* callee, whose
       parameters would otherwise be shadowed by whatever the last one left
       behind. When the names line up exactly — a self tail call, or two
       procedures that happen to share a parameter list, which is the shape of
       most loops — the clear is skipped and __scheme_env_define rebinds each
       box in place, so the iteration touches the arena not at all. */
    if (!__scheme_frame_fits_params(frame, closure))
    {
      sw_table_clear(&frame->bindings);
    }

    frame->parent = closure->env;
  }
  else
  {
    frame = arena_new(heap->arena, __scheme_env_t);
    if (frame == NULL)
    {
      return ERR_SCHEME_NOMEM;
    }

    TRY(__scheme_env_new(frame, heap, closure->env, __SCHEME_CALL_ENV_CAP));
  }

  size_t i = 0;
  for (__scheme_expression_t *p = closure->params;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr, ++i)
  {
    TRY(__scheme_env_define(frame, p->as.list.car->as.sym, args[i]));
  }

  /* __scheme_make_closure rejects an empty body, so `p` starts on a real cell
     and this loop always leaves one behind to park. */
  __scheme_expression_t *p = closure->body;
  for (; p->as.list.cdr->type == __SCHEME_EXPRESSION_TYPE_LIST; p = p->as.list.cdr)
  {
    scheme_value_t discarded = {0};
    TRY(__scheme_eval_expression(p->as.list.car, frame, depth + 1, &discarded));
  }

  tail->expr = p->as.list.car;
  tail->env = frame;
  return 0;
}

// Scheme's rule: #f is the only false value. Zero, in particular, is true.
PRIVATE uint8_t __scheme_is_truthy(scheme_value_t value)
{
  return !(value.tag == SCHEME_VALUE_TAG_BOOL && value.as.boolean == 0);
}

PRIVATE size_t __scheme_list_length(__scheme_expression_t *list)
{
  size_t n = 0;
  for (__scheme_expression_t *p = list;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr)
  {
    n++;
  }
  return n;
}

// NULL when `n` is past the end, which callers rule out with a length check.
PRIVATE __scheme_expression_t *__scheme_list_nth(__scheme_expression_t *list, size_t n)
{
  for (__scheme_expression_t *p = list;
       p->type == __SCHEME_EXPRESSION_TYPE_LIST;
       p = p->as.list.cdr)
  {
    if (n == 0)
    {
      return p->as.list.car;
    }
    n--;
  }
  return NULL;
}

/* `()` is not self-evaluating: it is an application with no procedure
   expression, which makes it an error rather than a nil value. */
PRIVATE int32_t __scheme_eval_nil_expression(scheme_value_t *output)
{
  UNUSED(output);
  return ERR_SCHEME_NOT_APPLICABLE;
}

PRIVATE int32_t __scheme_eval_symbol_expression(string_view_t symbol, __scheme_env_t *env, scheme_value_t *output)
{
  return __scheme_env_lookup(env, symbol, output);
}

PRIVATE int32_t __scheme_eval_number_expression(int32_t number, scheme_value_t *output)
{
  output->tag = SCHEME_VALUE_TAG_INT;
  output->as.val = number;
  return 0;
}

PRIVATE int32_t __scheme_eval_boolean_expression(uint8_t boolean, scheme_value_t *output)
{
  output->tag = SCHEME_VALUE_TAG_BOOL;
  output->as.boolean = boolean;
  return 0;
}

PRIVATE int32_t __scheme_env_new(__scheme_env_t *env, scheme_heap_t *heap, __scheme_env_t *parent, size_t cap)
{
  env->parent = parent;
  env->heap = heap;

  if (sw_table_init(&env->bindings, heap->arena, cap) != 0)
  {
    return ERR_SCHEME_NOMEM;
  }

  /* Registered as a root before it can hold anything. An env that the collector
     does not know about would have its bindings' values swept out from under
     it on the very next collection. */
  env->next_env = heap->envs;
  heap->envs = env;

  return 0;
}

/* The table borrows its value, so the binding is boxed in the arena rather than
   pointing at a caller's stack slot. Re-defining a name shadows within this
   scope only; parent scopes are untouched. */
PRIVATE int32_t __scheme_env_define(__scheme_env_t *env, string_view_t name, scheme_value_t value)
{
  /* Rebinding a name that this scope already holds writes through the existing
     box rather than bumping a second one. Nothing retains a box pointer —
     __scheme_env_lookup copies the value out — so the old box would be pure
     garbage, and in a recycled call frame it would be garbage produced once per
     iteration of a loop. */
  void *existing = NULL;
  if (sw_table_find(&env->bindings, name, &existing) == 1)
  {
    *(scheme_value_t *)existing = value;
    return 0;
  }

  scheme_value_t *boxed = arena_new(env->heap->arena, scheme_value_t);
  if (boxed == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  *boxed = value;

  if (sw_table_insert(&env->bindings, name, boxed) != 0)
  {
    return ERR_SCHEME_NOMEM;
  }

  return 0;
}

PRIVATE int32_t __scheme_env_lookup(__scheme_env_t *env, string_view_t name, scheme_value_t *out)
{
  for (__scheme_env_t *scope = env; scope != NULL; scope = scope->parent)
  {
    void *found = NULL;
    if (sw_table_find(&scope->bindings, name, &found) == 1)
    {
      *out = *(scheme_value_t *)found;
      return 0;
    }
  }

  return ERR_SCHEME_UNBOUND_SYMBOL;
}

PRIVATE int32_t __scheme_env_install_primitives(__scheme_env_t *env)
{
  struct {
    const char *name;
    scheme_primitive_fn_t fn;
  } primitives[] = {
    { "+",  __scheme_prim_add },
    { "-",  __scheme_prim_sub },
    { "*",  __scheme_prim_mul },
    { "=",  __scheme_prim_num_eq },
    { "<",  __scheme_prim_lt },
    { ">",  __scheme_prim_gt },
    { "<=", __scheme_prim_le },
    { ">=", __scheme_prim_ge },
    { "cons",  __scheme_prim_cons },
    { "car",   __scheme_prim_car },
    { "cdr",   __scheme_prim_cdr },
    { "null?", __scheme_prim_is_null },
    { "pair?", __scheme_prim_is_pair },
  };

  for (size_t i = 0; i < ARRAY_SIZE(primitives); ++i)
  {
    scheme_value_t value = {
      .tag = SCHEME_VALUE_TAG_PRIMITIVE,
      .as = { .prim = primitives[i].fn },
    };

    // The name is a string literal, so the view the table borrows outlives it.
    TRY(__scheme_env_define(env, string_view_from_cstr(primitives[i].name), value));
  }

  /* A binding rather than a literal, because there is no way to *write* the
     empty list yet: `()` is an application with no operator, and `'()` needs
     `quote`, which does not exist. Being an ordinary global means `(define nil
     5)` shadows it, unlike the special-form keywords. */
  scheme_value_t empty = { .tag = SCHEME_VALUE_TAG_NIL, .as = {0} };
  TRY(__scheme_env_define(env, string_view_from_cstr("nil"), empty));

  return 0;
}

/* Every argument must already be an integer. Overflow is reported rather than
   wrapping, since signed overflow is undefined behaviour. */
PRIVATE int32_t __scheme_prim_require_ints(const scheme_value_t *args, size_t argc)
{
  for (size_t i = 0; i < argc; ++i)
  {
    if (args[i].tag != SCHEME_VALUE_TAG_INT)
    {
      return ERR_SCHEME_TYPE_ERROR;
    }
  }

  return 0;
}

// (+) is 0, the identity for addition.
PRIVATE int32_t __scheme_prim_add(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  TRY(__scheme_prim_require_ints(args, argc));

  int32_t acc = 0;
  for (size_t i = 0; i < argc; ++i)
  {
    if (__builtin_add_overflow(acc, args[i].as.val, &acc))
    {
      return ERR_SCHEME_OVERFLOW;
    }
  }

  output->tag = SCHEME_VALUE_TAG_INT;
  output->as.val = acc;
  return 0;
}

// (- x) negates; (- x y ...) subtracts the rest from the first. (-) is an error.
PRIVATE int32_t __scheme_prim_sub(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  TRY(__scheme_prim_require_ints(args, argc));

  if (argc == 0)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  int32_t acc = 0;
  size_t start = 0;

  if (argc == 1)
  {
    if (__builtin_sub_overflow(0, args[0].as.val, &acc))
    {
      return ERR_SCHEME_OVERFLOW;
    }
  }
  else
  {
    acc = args[0].as.val;
    start = 1;

    for (size_t i = start; i < argc; ++i)
    {
      if (__builtin_sub_overflow(acc, args[i].as.val, &acc))
      {
        return ERR_SCHEME_OVERFLOW;
      }
    }
  }

  output->tag = SCHEME_VALUE_TAG_INT;
  output->as.val = acc;
  return 0;
}

// (*) is 1, the identity for multiplication.
PRIVATE int32_t __scheme_prim_mul(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  TRY(__scheme_prim_require_ints(args, argc));

  int32_t acc = 1;
  for (size_t i = 0; i < argc; ++i)
  {
    if (__builtin_mul_overflow(acc, args[i].as.val, &acc))
    {
      return ERR_SCHEME_OVERFLOW;
    }
  }

  output->tag = SCHEME_VALUE_TAG_INT;
  output->as.val = acc;
  return 0;
}

/* Comparisons chain, as in Scheme: `(< 1 2 3)` is true because each adjacent
   pair is ordered, not because the first and last are. A single argument has no
   pair to compare, so two is the minimum. */
typedef uint8_t (*__scheme_int_cmp_fn_t)(int32_t a, int32_t b);

PRIVATE int32_t __scheme_prim_compare(const scheme_value_t *args, size_t argc, __scheme_int_cmp_fn_t pred, scheme_value_t *output)
{
  TRY(__scheme_prim_require_ints(args, argc));

  if (argc < 2)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  uint8_t result = 1;
  for (size_t i = 1; i < argc; ++i)
  {
    if (!pred(args[i - 1].as.val, args[i].as.val))
    {
      result = 0;
      break;
    }
  }

  output->tag = SCHEME_VALUE_TAG_BOOL;
  output->as.boolean = result;
  return 0;
}

PRIVATE uint8_t __scheme_cmp_eq(int32_t a, int32_t b) { return a == b; }
PRIVATE uint8_t __scheme_cmp_lt(int32_t a, int32_t b) { return a < b; }
PRIVATE uint8_t __scheme_cmp_gt(int32_t a, int32_t b) { return a > b; }
PRIVATE uint8_t __scheme_cmp_le(int32_t a, int32_t b) { return a <= b; }
PRIVATE uint8_t __scheme_cmp_ge(int32_t a, int32_t b) { return a >= b; }

PRIVATE int32_t __scheme_prim_num_eq(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  return __scheme_prim_compare(args, argc, __scheme_cmp_eq, output);
}

PRIVATE int32_t __scheme_prim_lt(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  return __scheme_prim_compare(args, argc, __scheme_cmp_lt, output);
}

PRIVATE int32_t __scheme_prim_gt(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  return __scheme_prim_compare(args, argc, __scheme_cmp_gt, output);
}

PRIVATE int32_t __scheme_prim_le(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  return __scheme_prim_compare(args, argc, __scheme_cmp_le, output);
}

PRIVATE int32_t __scheme_prim_ge(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);
  return __scheme_prim_compare(args, argc, __scheme_cmp_ge, output);
}

/* The one place a runtime object is born, and therefore the only place a
   collection can start. Note the order: the free list is tried first, a
   collection is only attempted when it is empty, and a new page is only taken
   from the arena when collecting did not produce anything. That ordering is
   what makes the heap grow to the program's live size rather than its
   allocation count. */
PRIVATE void *__scheme_heap_alloc(scheme_heap_t *heap, __scheme_obj_kind_t kind)
{
  if (heap->free_list == NULL)
  {
    /* The threshold is what stops a program whose live set genuinely grows from
       collecting once per page and recovering nothing. Without it, building a
       list of N cells collects N/page_objects times and frees nothing on any of
       them, which is quadratic in N for no benefit. Doubling after each
       collection makes the number of collections logarithmic in the live size,
       while a program whose live set is flat — the case collection actually
       helps — still collects on every exhausted free list. */
    if (!heap->gc_disabled && heap->stats.objects_live >= heap->gc_threshold)
    {
      __scheme_gc_collect(heap);

      heap->gc_threshold = heap->stats.objects_live * 2;
      if (heap->gc_threshold < heap->page_objects)
      {
        heap->gc_threshold = heap->page_objects;
      }
    }

    if (heap->free_list == NULL && __scheme_heap_add_page(heap) != 0)
    {
      return NULL;
    }
  }

  __scheme_obj_header_t *header = heap->free_list;
  heap->free_list = header->next_free;

  /* The slot may hold a dead object's fields. Nothing reads them before they
     are assigned, but a stale value left in an unassigned field would be
     traced as live on the next collection and keep real garbage alive. */
  memset(header, 0, __SCHEME_SLOT_SIZE);
  header->kind = (uint8_t)kind;

  heap->stats.objects_allocated++;
  heap->stats.objects_live++;
  return header;
}

PRIVATE int32_t __scheme_heap_add_page(scheme_heap_t *heap)
{
  __scheme_page_t *page = arena_new(heap->arena, __scheme_page_t);
  if (page == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  page->slots = arena_alloc(heap->arena, (ptrdiff_t)__SCHEME_SLOT_SIZE,
                            __SCHEME_SLOT_ALIGN, (ptrdiff_t)heap->page_objects);
  if (page->slots == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  page->slot_count = heap->page_objects;
  page->next = heap->pages;
  heap->pages = page;

  /* arena_alloc zeroes, and zero is __SCHEME_OBJ_FREE, so the slots are already
     classified correctly; all that remains is to thread them onto the list. */
  for (size_t i = 0; i < page->slot_count; ++i)
  {
    __scheme_obj_header_t *header = (__scheme_obj_header_t *)(page->slots + i * __SCHEME_SLOT_SIZE);
    header->next_free = heap->free_list;
    heap->free_list = header;
  }

  heap->stats.pages++;
  return 0;
}

PRIVATE void __scheme_gc_collect(scheme_heap_t *heap)
{
#if defined(__SANITIZE_ADDRESS__)
  /* If the fake stack is active despite __asan_default_options — someone set
     ASAN_OPTIONS explicitly — the roots on the machine stack are not there to
     be found. Declining to collect is the only safe response: the heap grows,
     which is a resource problem, whereas collecting on an empty root set frees
     live objects, which is a wrong answer. */
  if (__asan_get_current_fake_stack() != NULL)
  {
    return;
  }
#endif

  heap->stats.collections++;
  __scheme_gc_mark_roots(heap);
  __scheme_gc_sweep(heap);
}

/* setjmp is not being used for control flow here. It is the portable way to
   force the callee-saved registers into memory, and specifically into *this*
   frame, so that a value held only in a register is found by the stack scan
   below. Without it, an optimising build can keep the only reference to a live
   object in a register and the collector will free it. */
PRIVATE void __scheme_gc_mark_roots(scheme_heap_t *heap)
{
  jmp_buf registers;
  (void)setjmp(registers);

  /* `registers` is the lowest thing in this frame that matters, so taking its
     address gives a bound that covers it along with every evaluator frame
     below the base. */
  __scheme_gc_mark_stack(heap, (const char *)&registers, heap->stack_base);

  /* The result of the program so far. It lives in the caller's frame, which is
     above stack_base and therefore outside the scanned region — this is the one
     root conservative scanning structurally cannot find. */
  if (heap->result_slot != NULL)
  {
    __scheme_gc_mark_value(heap, *heap->result_slot);
  }

  for (__scheme_env_t *env = heap->envs; env != NULL; env = env->next_env)
  {
    sw_table_iter_t iter = {0};
    sw_table_iter_new(&iter, &env->bindings);

    void *boxed = NULL;
    while (sw_table_iter_next(&iter, NULL, &boxed) == 1)
    {
      __scheme_gc_mark_value(heap, *(scheme_value_t *)boxed);
    }
  }
}

/* Reading arbitrary stack words is exactly what a conservative collector does,
   and exactly what the sanitizers exist to complain about: the region between
   frames contains ASan redzones and bytes no one has written. The reads are
   deliberate and in-bounds for the stack mapping, so the instrumentation is
   turned off for this function only. */
__attribute__((no_sanitize_address))
PRIVATE void __scheme_gc_mark_stack(scheme_heap_t *heap, const char *low, const char *high)
{
  /* Anything that is really a pointer was stored at a pointer-aligned address,
     so a misaligned start would scan a shifted view of every word and find
     nothing. */
  uintptr_t start = ((uintptr_t)low + sizeof(void *) - 1) & ~(uintptr_t)(sizeof(void *) - 1);

  for (const char *p = (const char *)start; p + sizeof(void *) <= high; p += sizeof(void *))
  {
    void *candidate = NULL;
    memcpy(&candidate, p, sizeof candidate);
    __scheme_gc_mark_address(heap, candidate);
  }
}

/* The conservative test. An address counts as a root only if it lands exactly
   on the start of a slot in one of our pages and that slot holds a live object.
   Interior pointers are rejected: nothing in the evaluator ever holds one, and
   accepting them would mean a stray integer that happens to fall inside a page
   could resurrect an arbitrary neighbouring object.

   A stray integer landing exactly on a slot boundary still retains that object
   for one cycle. That is the price of conservatism, and it is a leak rather
   than a correctness bug — objects are never freed while a plausible reference
   exists, only kept longer than necessary. */
PRIVATE void __scheme_gc_mark_address(scheme_heap_t *heap, const void *addr)
{
  for (__scheme_page_t *page = heap->pages; page != NULL; page = page->next)
  {
    if ((const char *)addr < page->slots)
    {
      continue;
    }

    size_t offset = (size_t)((const char *)addr - page->slots);
    if (offset >= page->slot_count * __SCHEME_SLOT_SIZE)
    {
      continue;
    }

    if (offset % __SCHEME_SLOT_SIZE != 0)
    {
      return;
    }

    __scheme_obj_header_t *header = (__scheme_obj_header_t *)(page->slots + offset);
    switch (header->kind)
    {
    case __SCHEME_OBJ_PAIR:
    {
      scheme_value_t value = { .tag = SCHEME_VALUE_TAG_PAIR, .as = { .pair = (scheme_pair_t *)header } };
      __scheme_gc_mark_value(heap, value);
    }; break;
    case __SCHEME_OBJ_CLOSURE:
    {
      scheme_value_t value = { .tag = SCHEME_VALUE_TAG_CLOSURE, .as = { .closure = (scheme_closure_t *)header } };
      __scheme_gc_mark_value(heap, value);
    }; break;
    default: break;
    }

    return;
  }
}

/* Marking iterates along the cdr and recurses only into the car. A list is a
   cdr chain, so recursing on both would put the C stack in proportion to the
   list's *length*, and a list long enough to be worth collecting is long enough
   to overflow it. Recursion depth is now the nesting depth of cars instead,
   which is bounded by how deeply nested the data actually is. */
PRIVATE void __scheme_gc_mark_value(scheme_heap_t *heap, scheme_value_t value)
{
  for (;;)
  {
    if (value.tag == SCHEME_VALUE_TAG_PAIR)
    {
      scheme_pair_t *pair = value.as.pair;
      if (pair->header.mark)
      {
        return;
      }

      pair->header.mark = 1;
      __scheme_gc_mark_value(heap, pair->car);
      value = pair->cdr;
      continue;
    }

    if (value.tag == SCHEME_VALUE_TAG_CLOSURE)
    {
      scheme_closure_t *closure = value.as.closure;
      if (closure->header.mark)
      {
        return;
      }

      closure->header.mark = 1;
      /* No need to trace closure->env: environments are never collected, and
         every one of them is walked as a root already. */
      return;
    }

    // Integers, booleans, nil and primitives are not heap objects.
    return;
  }
}

/* The free list is rebuilt from scratch rather than appended to, so a slot can
   only ever appear on it once — the alternative is threading newly freed slots
   onto a list that already contains the untouched ones, which is the same work
   with a way to get it wrong. */
PRIVATE void __scheme_gc_sweep(scheme_heap_t *heap)
{
  heap->free_list = NULL;
  heap->stats.objects_live = 0;

  for (__scheme_page_t *page = heap->pages; page != NULL; page = page->next)
  {
    for (size_t i = 0; i < page->slot_count; ++i)
    {
      __scheme_obj_header_t *header = (__scheme_obj_header_t *)(page->slots + i * __SCHEME_SLOT_SIZE);

      if (header->kind != __SCHEME_OBJ_FREE)
      {
        if (header->mark)
        {
          header->mark = 0;
          heap->stats.objects_live++;
          continue;
        }

        header->kind = __SCHEME_OBJ_FREE;
        heap->stats.objects_freed++;
      }

      header->next_free = heap->free_list;
      heap->free_list = header;
    }
  }
}

/* `(cons a b)` is the only constructor in the language, and the only primitive
   that allocates. It is strict in neither field's *shape*: the cdr need not be
   a list, so `(cons 1 2)` is a perfectly legal improper pair. */
PRIVATE int32_t __scheme_prim_cons(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  if (argc != 2)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  scheme_pair_t *pair = __scheme_heap_alloc(heap, __SCHEME_OBJ_PAIR);
  if (pair == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  pair->car = args[0];
  pair->cdr = args[1];

  output->tag = SCHEME_VALUE_TAG_PAIR;
  output->as.pair = pair;
  return 0;
}

PRIVATE int32_t __scheme_prim_car(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);

  if (argc != 1)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  return scheme_car(args[0], output);
}

PRIVATE int32_t __scheme_prim_cdr(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);

  if (argc != 1)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  return scheme_cdr(args[0], output);
}

/* `null?` is true of the empty list only. It is emphatically not the complement
   of `pair?`: a number is neither. */
PRIVATE int32_t __scheme_prim_is_null(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);

  if (argc != 1)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  output->tag = SCHEME_VALUE_TAG_BOOL;
  output->as.boolean = (args[0].tag == SCHEME_VALUE_TAG_NIL);
  return 0;
}

PRIVATE int32_t __scheme_prim_is_pair(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output)
{
  UNUSED(heap);

  if (argc != 1)
  {
    return ERR_SCHEME_BAD_ARITY;
  }

  output->tag = SCHEME_VALUE_TAG_BOOL;
  output->as.boolean = (args[0].tag == SCHEME_VALUE_TAG_PAIR);
  return 0;
}

int32_t scheme_car(scheme_value_t pair, scheme_value_t *output)
{
  if (pair.tag != SCHEME_VALUE_TAG_PAIR)
  {
    return ERR_SCHEME_NOT_A_PAIR;
  }

  *output = pair.as.pair->car;
  return 0;
}

int32_t scheme_cdr(scheme_value_t pair, scheme_value_t *output)
{
  if (pair.tag != SCHEME_VALUE_TAG_PAIR)
  {
    return ERR_SCHEME_NOT_A_PAIR;
  }

  *output = pair.as.pair->cdr;
  return 0;
}

PRIVATE int32_t __scheme_parser_new(__scheme_parser_t *parser, __scheme_lexer_t lexer, arena_t *arena)
{
  parser->arena = arena;
  parser->lexer = lexer;

  TRY(__scheme_lexer_next_token(&parser->lexer, &parser->curr_token));
  TRY(__scheme_lexer_next_token(&parser->lexer, &parser->peek_token));
  return 0;
}

PRIVATE int32_t __scheme_parser_advance(__scheme_parser_t *parser)
{
  parser->curr_token = parser->peek_token;
  TRY(__scheme_lexer_next_token(&parser->lexer, &parser->peek_token));
  return 0;
}

PRIVATE int32_t __scheme_parser_alloc_expression(__scheme_parser_t *parser, __scheme_expression_t **out)
{
  __scheme_expression_t *expr = arena_new(parser->arena, __scheme_expression_t);
  if (expr == NULL)
  {
    return ERR_SCHEME_NOMEM;
  }

  *out = expr;
  return 0;
}

// Every successful path here must consume at least one token, otherwise the
// driving loop in scheme_eval never terminates.
PRIVATE int32_t __scheme_parser_parse_expression(__scheme_parser_t *parser, __scheme_expression_t **out)
{
  switch (parser->curr_token.type)
  {
  case __SCHEME_TOKEN_TYPE_NUMBER:
  {
    int32_t value = {0};
    TRY(string_view_into_i32(parser->curr_token.value, &value));

    __scheme_expression_t *expr = NULL;
    TRY(__scheme_parser_alloc_expression(parser, &expr));
    expr->type = __SCHEME_EXPRESSION_TYPE_NUMBER;
    expr->as.num = value;

    TRY(__scheme_parser_advance(parser));

    *out = expr;
  }; break;
  case __SCHEME_TOKEN_TYPE_BOOLEAN:
  {
    __scheme_expression_t *expr = NULL;
    TRY(__scheme_parser_alloc_expression(parser, &expr));
    expr->type = __SCHEME_EXPRESSION_TYPE_BOOLEAN;
    // The lexer only ever produces "#t" or "#f", so the second byte decides.
    expr->as.boolean = parser->curr_token.value.addr[1] == 't';

    TRY(__scheme_parser_advance(parser));

    *out = expr;
  }; break;
  case __SCHEME_TOKEN_TYPE_SYMBOL:
  {
    __scheme_expression_t *expr = NULL;
    TRY(__scheme_parser_alloc_expression(parser, &expr));
    expr->type = __SCHEME_EXPRESSION_TYPE_SYMBOL;
    expr->as.sym = parser->curr_token.value;

    TRY(__scheme_parser_advance(parser));

    *out = expr;
  }; break;
  case __SCHEME_TOKEN_TYPE_LPAREN:
  {
    // Consume '(' so parse_list starts on the first element and needs no
    // special case for it.
    TRY(__scheme_parser_advance(parser));
    TRY(__scheme_parser_parse_list(parser, out));
  }; break;
  case __SCHEME_TOKEN_TYPE_RPAREN:
    return ERR_SCHEME_UNEXPECTED_RPAREN;
  case __SCHEME_TOKEN_TYPE_EOF:
    return ERR_SCHEME_UNEXPECTED_EOF;
  default:
    return ERR_SCHEME_INVALID_TOKEN;
  }

  return 0;
}

// Builds a nil-terminated cons chain. `tail` always points at the slot that
// should receive the next cell: initially `head` itself, so an empty list stays
// nil, and thereafter the previous cell's cdr. That keeps appending to one
// store, with no list walk and no branch on emptiness.
PRIVATE int32_t __scheme_parser_parse_list(__scheme_parser_t *parser, __scheme_expression_t **out)
{
  __scheme_expression_t *head = &__scheme_nil;
  __scheme_expression_t **tail = &head;

  while (parser->curr_token.type != __SCHEME_TOKEN_TYPE_RPAREN)
  {
    if (parser->curr_token.type == __SCHEME_TOKEN_TYPE_EOF)
    {
      return ERR_SCHEME_UNTERMINATED_LIST;
    }

    __scheme_expression_t *car = NULL;
    TRY(__scheme_parser_parse_expression(parser, &car));

    __scheme_expression_t *cell = NULL;
    TRY(__scheme_parser_alloc_expression(parser, &cell));
    cell->type = __SCHEME_EXPRESSION_TYPE_LIST;
    cell->as.list.car = car;
    cell->as.list.cdr = &__scheme_nil;

    *tail = cell;
    tail = &cell->as.list.cdr;
  }

  TRY(__scheme_parser_advance(parser));   // consume ')'

  *out = head;
  return 0;
}

PRIVATE uint8_t __scheme_parser_is_done(__scheme_parser_t *p)
{
  return p->curr_token.type == __SCHEME_TOKEN_TYPE_EOF;
}


PRIVATE void __scheme_lexer_new(__scheme_lexer_t *lexer, string_view_t file)
{
  lexer->input = file;
  lexer->position = 0;
  lexer->read_position = 0;
  lexer->ch = 0;
  lexer->column = 1;
  lexer->line = 1;
  __scheme_lexer_read_char(lexer);
}

PRIVATE void __scheme_lexer_read_char(__scheme_lexer_t *lexer)
{
  if (lexer->ch == '\n')
  {
    lexer->line++;
    lexer->column = 1;
  }
  else if (lexer->ch != '\0')
  {
    lexer->column++;
  }

  if (lexer->read_position >= lexer->input.length)
  {
    lexer->ch = 0;
  }
  else
  {
    lexer->ch = lexer->input.addr[lexer->read_position];

  }

  lexer->position = lexer->read_position;
  lexer->read_position++;
}

PRIVATE int32_t __scheme_lexer_next_token(__scheme_lexer_t *lexer, __scheme_token_t *tok)
{
  tok->type = __SCHEME_TOKEN_TYPE_NONE;
  tok->value = string_view_empty();

  __scheme_lexer_skip_whitespace(lexer);

  tok->line = lexer->line;
  tok->column = lexer->column;

  switch (lexer->ch)
  {
  case 0:
  {
    tok->type = __SCHEME_TOKEN_TYPE_EOF;
    tok->value = string_view_empty();
  }; break;
  case '(':
  {
    tok->type = __SCHEME_TOKEN_TYPE_LPAREN;
    tok->value = string_view_from_cstr("(");
  }; break;
  case ')':
  {
    tok->type = __SCHEME_TOKEN_TYPE_RPAREN;
    tok->value = string_view_from_cstr(")");
  }; break;
  case '+':
  {
    tok->type = __SCHEME_TOKEN_TYPE_SYMBOL;
    tok->value = string_view_from_cstr("+");
  }; break;
  case '#':
  {
    // Only #t and #f exist for now, and both must stand alone: "#tx" is a
    // typo, not a boolean followed by a symbol.
    size_t pos = lexer->position;
    __scheme_lexer_read_char(lexer);

    if (lexer->ch != 't' && lexer->ch != 'f')
    {
      return ERR_SCHEME_INVALID_TOKEN;
    }
    __scheme_lexer_read_char(lexer);

    if (!__is_delimiter(lexer->ch))
    {
      return ERR_SCHEME_INVALID_TOKEN;
    }

    tok->type = __SCHEME_TOKEN_TYPE_BOOLEAN;
    tok->value = (string_view_t){
      .addr = lexer->input.addr + pos,
      .length = lexer->position - pos,
    };
    return 0;
  }; break;
  case '-':
  {
    if (__is_digit(__scheme_lexer_peek_char(lexer)))
    {
      __scheme_lexer_read_num(lexer, &tok->value);
      tok->type = __SCHEME_TOKEN_TYPE_NUMBER;
      return 0;
    }

    tok->type = __SCHEME_TOKEN_TYPE_SYMBOL;
    tok->value = string_view_from_cstr("-");
  }; break;
  default:
  {
    if (__is_digit(lexer->ch))
    {
      __scheme_lexer_read_num(lexer, &tok->value);
      tok->type = __SCHEME_TOKEN_TYPE_NUMBER;
      return 0;
    }

    // Every delimiter is either handled by a case above or already skipped as
    // whitespace, so this always consumes at least one character.
    __scheme_lexer_read_symbol(lexer, &tok->value);
    tok->type = __SCHEME_TOKEN_TYPE_SYMBOL;
    return 0;
  }; break;
  }

  __scheme_lexer_read_char(lexer);
  return 0;
}

PRIVATE void __scheme_lexer_read_num(__scheme_lexer_t *lexer, string_view_t *value)
{
  size_t pos = lexer->position;

  // negative number handling
  if (lexer->ch == '-')
  {
    __scheme_lexer_read_char(lexer);
  }

  while (__is_digit(lexer->ch))
  {
    __scheme_lexer_read_char(lexer);
  }

  value->addr = lexer->input.addr + pos;
  value->length = lexer->position - pos;
}

PRIVATE void __scheme_lexer_read_symbol(__scheme_lexer_t *lexer, string_view_t *value)
{
  size_t pos = lexer->position;

  while (!__is_delimiter(lexer->ch))
  {
    __scheme_lexer_read_char(lexer);
  }

  value->addr = lexer->input.addr + pos;
  value->length = lexer->position - pos;
}

PRIVATE char __scheme_lexer_peek_char(__scheme_lexer_t *lexer)
{
  if (lexer->read_position >= lexer->input.length)
  {
    return 0;
  }

  return lexer->input.addr[lexer->read_position];
}

PRIVATE void __scheme_lexer_skip_whitespace(__scheme_lexer_t *lexer)
{
  while (lexer->ch == ' ' || lexer->ch == '\t' || lexer->ch == '\n' || lexer->ch == '\r')
  {
    __scheme_lexer_read_char(lexer);
  }
}

PRIVATE uint8_t __is_digit(char c)
{
  return c >= '0' && c <= '9';
}

// Characters that terminate a symbol. Scheme identifiers are permissive, so a
// symbol is defined by what ends it rather than by an allowed-character set.
PRIVATE uint8_t __is_delimiter(char c)
{
  return c == '\0' || c == ' ' || c == '\t' || c == '\n' || c == '\r'
      || c == '(' || c == ')';
}

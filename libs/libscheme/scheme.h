#ifndef SKENE_SCHEME_H
#define SKENE_SCHEME_H

#include <common/common.h>
#include <libstrview/string_view.h>
#include <libarena/arena.h>

typedef enum {
  SCHEME_VALUE_TAG_INT,
  SCHEME_VALUE_TAG_BOOL,
  SCHEME_VALUE_TAG_PRIMITIVE,
  SCHEME_VALUE_TAG_CLOSURE,
  /* The empty list. A value in its own right, unlike the `()` *expression*,
     which is an application with no operator and therefore an error. */
  SCHEME_VALUE_TAG_NIL,
  SCHEME_VALUE_TAG_PAIR,
} scheme_value_tag_t;

typedef struct scheme_value_t scheme_value_t;

/* Where runtime objects come from: a mark-sweep collector over a free list of
   fixed-size slots. Pairs and closures live here and are reclaimed when
   unreachable. Environments do not — they are bump-allocated and permanently
   live, so anything their bindings hold is retained. The handle is opaque and
   threaded through every allocating operation, so the storage strategy stays
   one implementation's business rather than the evaluator's. */
typedef struct scheme_heap_t scheme_heap_t;

/* A builtin procedure. Arguments arrive already evaluated. `heap` is where a
   primitive that constructs a value — `cons` is the only one so far — gets its
   storage; primitives that merely inspect their arguments ignore it. */
typedef int32_t (*scheme_primitive_fn_t)(scheme_heap_t *heap, const scheme_value_t *args, size_t argc, scheme_value_t *output);

/* A user procedure: parameters, body and the environment it closed over. The
   representation is private to the evaluator, and every part of it lives in the
   arena passed to scheme_eval, so a closure is only valid for as long as that
   arena is. */
typedef struct scheme_closure_t scheme_closure_t;

/* A mutable cons cell. Opaque for the same reason as the heap: a moving
   collector would need every reference to a pair to go through the runtime. */
typedef struct scheme_pair_t scheme_pair_t;

struct scheme_value_t {
  scheme_value_tag_t tag;
  union {
    int32_t val;
    uint8_t boolean;
    scheme_primitive_fn_t prim;
    scheme_closure_t *closure;
    scheme_pair_t *pair;
  } as;
};

/* Field accessors for a SCHEME_VALUE_TAG_PAIR value, so a caller can walk a
   list without seeing the cell layout. Both return ERR_SCHEME_NOT_A_PAIR for
   anything else, including nil — `(car '())` is an error in Scheme. */
int32_t scheme_car(scheme_value_t pair, scheme_value_t *output);
int32_t scheme_cdr(scheme_value_t pair, scheme_value_t *output);

#define ERR_SCHEME_INVALID_TOKEN 1
#define ERR_SCHEME_UNEXPECTED_RPAREN 2
#define ERR_SCHEME_UNEXPECTED_EOF 3
#define ERR_SCHEME_UNTERMINATED_LIST 4
#define ERR_SCHEME_NOMEM 5
/* The head of a list evaluated to something that is not a procedure. Also
   covers the empty list, which has no procedure expression at all. */
#define ERR_SCHEME_NOT_APPLICABLE 6
#define ERR_SCHEME_UNBOUND_SYMBOL 7
#define ERR_SCHEME_BAD_ARITY 8
#define ERR_SCHEME_TOO_MANY_ARGS 9
#define ERR_SCHEME_OVERFLOW 10
/* A special form was written with operands it cannot accept, e.g. `(if)` or
   `(lambda (1) x)`. Distinct from BAD_ARITY, which is a runtime property of a
   procedure call; this is a property of the source text. */
#define ERR_SCHEME_BAD_SYNTAX 11
/* A procedure was handed a value of the wrong type, e.g. `(+ #t 1)`. */
#define ERR_SCHEME_TYPE_ERROR 12
/* Evaluation nested deeper than __SCHEME_MAX_DEPTH. Only *non-tail* nesting is
   counted — a tail call iterates rather than nesting — so this bounds genuine
   recursion depth, and unbounded non-tail recursion is reported rather than
   allowed to overflow the C stack. */
#define ERR_SCHEME_DEPTH_EXCEEDED 13
/* `car` or `cdr` was applied to something that is not a cons cell. Separate
   from TYPE_ERROR so that walking off the end of a list — the overwhelmingly
   common case, where the offender is nil — is distinguishable from `(car 1)`. */
#define ERR_SCHEME_NOT_A_PAIR 14

/* How the collector behaves. It is deliberately separate from the evaluator:
   when a collection happens, and how much memory it is allowed to keep, are
   policy, and policy is the caller's. What is *not* separable is the root set —
   only the evaluator knows which values are live, so that part is evaluator
   code and always will be. */
typedef struct {
  /* Objects per heap page. A page is bump-allocated from the arena when the
     free list comes up empty after a collection, and is never given back, so
     this is the granularity at which the heap grows. Zero selects the default. */
  size_t page_objects;
  /* When set, the collector never runs and the heap only ever grows. Useful to
     measure what the collector is actually buying, and to rule it out when
     something else is suspected. */
  uint8_t gc_disabled;
} scheme_config_t;

/* Cumulative except for `objects_live` and `pages`, which are current. */
typedef struct {
  size_t collections;
  size_t objects_allocated;
  size_t objects_freed;
  size_t objects_live;
  size_t pages;
} scheme_gc_stats_t;

int32_t scheme_eval(arena_t *arena, string_view_t input, scheme_value_t *output);
/* As scheme_eval, but with explicit policy. `config` and `stats` may each be
   NULL, selecting the defaults and discarding the statistics respectively. */
int32_t scheme_eval_with_config(arena_t *arena, string_view_t input, const scheme_config_t *config, scheme_value_t *output, scheme_gc_stats_t *stats);


#endif

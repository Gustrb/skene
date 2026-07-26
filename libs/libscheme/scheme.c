#include "libstrview/string_view.h"
#include <libscheme/scheme.h>

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
  __SCHEME_TOKEN_TYPE_LPAREN,
  __SCHEME_TOKEN_TYPE_RPAREN,
  __SCHEME_TOKEN_TYPE_EOF,

  __TOML_TOKEN_TYPE_COUNT
} __scheme_token_type_t;

typedef struct {
  string_view_t value;
  __scheme_token_type_t type;

  size_t line;
  size_t column;
} __scheme_token_t;

typedef struct {
  arena_t arena;
  __scheme_lexer_t lexer;

  __scheme_token_t curr_token;
  __scheme_token_t peek_token;
} __scheme_parser_t;

PRIVATE uint8_t __is_digit(char c);
PRIVATE void __scheme_lexer_new(__scheme_lexer_t *lexer, string_view_t file);
PRIVATE void __scheme_lexer_read_char(__scheme_lexer_t *lexer);
PRIVATE int32_t __scheme_lexer_next_token(__scheme_lexer_t *lexer, __scheme_token_t *tok);
PRIVATE void __scheme_lexer_skip_whitespace(__scheme_lexer_t *lexer);
PRIVATE void __scheme_lexer_read_num(__scheme_lexer_t *lexer, string_view_t *);

typedef enum {
  __SCHEME_EXPRESSION_TYPE_NUMBER,
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
    __scheme_cons_t list;
  } as;
} __scheme_expression_t;

PRIVATE int32_t __scheme_parser_new(__scheme_parser_t *parser, __scheme_lexer_t lexer, arena_t arena);
PRIVATE int32_t __scheme_parser_advance(__scheme_parser_t *parser);
PRIVATE int32_t __scheme_parser_parse_expression(__scheme_parser_t *, __scheme_expression_t *expr);
PRIVATE uint8_t __scheme_parser_is_done(__scheme_parser_t *p);

PRIVATE int32_t __scheme_eval_expression(__scheme_expression_t *expr, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_list_expression(__scheme_cons_t list, scheme_value_t *output);
PRIVATE int32_t __scheme_eval_nil_expression(scheme_value_t *output);
PRIVATE int32_t __scheme_eval_number_expression(int32_t number, scheme_value_t *output);

/*
  PROGRAM    = EXPRESSION*
  EXPRESSION = NUMBER | LIST
  LIST       = '(' EXPRESSION* ')'
*/

int32_t scheme_eval(arena_t arena, string_view_t input, scheme_value_t *output)
{
  __scheme_lexer_t lexer = {0};
  __scheme_lexer_new(&lexer, input);

  __scheme_parser_t parser = {0};
  TRY(__scheme_parser_new(&parser, lexer, arena));

  __scheme_expression_t expr = {0};
  while (!__scheme_parser_is_done(&parser))
  {
    TRY(__scheme_parser_parse_expression(&parser, &expr));
    TRY(__scheme_eval_expression(&expr, output));
  }

  return 0;
}

PRIVATE int32_t __scheme_eval_expression(__scheme_expression_t *expr, scheme_value_t *output)
{
  switch (expr->type)
  {
  case __SCHEME_EXPRESSION_TYPE_LIST:
  {
    TRY(__scheme_eval_list_expression(expr->as.list, output));
  }; break;
  case __SCHEME_EXPRESSION_TYPE_NIL:
  {
    TRY(__scheme_eval_nil_expression(output));
  }; break;
  case __SCHEME_EXPRESSION_TYPE_NUMBER:
  {
    TRY(__scheme_eval_number_expression(expr->as.num, output));
  }; break;
  }
  return 0;
}

PRIVATE int32_t __scheme_eval_list_expression(__scheme_cons_t list, scheme_value_t *output)
{
  UNUSED(list), UNUSED(output);
  return 0;
}

PRIVATE int32_t __scheme_eval_nil_expression(scheme_value_t *output)
{
  UNUSED(output);
  return 0;
}

PRIVATE int32_t __scheme_eval_number_expression(int32_t number, scheme_value_t *output)
{
  output->tag = SCHEME_VALUE_TAG_INT;
  output->as.val = number;
  return 0;
}

PRIVATE int32_t __scheme_parser_new(__scheme_parser_t *parser, __scheme_lexer_t lexer, arena_t arena)
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

PRIVATE int32_t __scheme_parser_parse_expression(__scheme_parser_t *parser, __scheme_expression_t *expr)
{
    if (parser->curr_token.type != __SCHEME_TOKEN_TYPE_NUMBER)
    {
      // TODO: add proper error handling
      return ERR_SCHEME_INVALID_TOKEN;
    }

    int32_t value = {0};
    TRY(string_view_into_i32(parser->curr_token.value, &value));

    expr->as.num = value;
    expr->type = __SCHEME_EXPRESSION_TYPE_NUMBER;

    TRY(__scheme_parser_advance(parser));

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
  default:
  {
    if (__is_digit(lexer->ch))
    {
      __scheme_lexer_read_num(lexer, &tok->value);
      tok->type = __SCHEME_TOKEN_TYPE_NUMBER;
      return 0;
    }
  }; break;
  }

  __scheme_lexer_read_char(lexer);
  return 0;
}

PRIVATE void __scheme_lexer_read_num(__scheme_lexer_t *lexer, string_view_t *value)
{
  size_t pos = lexer->position;

  while (__is_digit(lexer->ch))
  {
    __scheme_lexer_read_char(lexer);
  }

  value->addr = lexer->input.addr + pos;
  value->length = lexer->position - pos;
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

#include "libstrview/string_view.h"
#include <libencoding/toml/toml_parser.h>

void toml_parser_new(toml_parser_t *p, string_view_t file)
{
  p->file = file;
  p->offset = 0;
  p->__diagnostics_len = 0;
}

typedef struct {
  string_view_t input;
  size_t position;
  size_t read_position;
  char ch;

  size_t line;
  size_t column;
} __toml_lexer_t;

typedef enum {
  __TOML_TOKEN_TYPE_NONE = 0,
  __TOML_TOKEN_TYPE_LBRACKET,
  __TOML_TOKEN_TYPE_RBRACKET,
  __TOML_TOKEN_TYPE_IDENTIFIER,
  __TOML_TOKEN_TYPE_EOF,

  __TOML_TOKEN_TYPE_COUNT
} __toml_token_type_t;

typedef struct {
  string_view_t value;
  __toml_token_type_t type;

  size_t line;
  size_t column;
} __toml_token_t;

PRIVATE uint8_t __is_letter(char c);

PRIVATE void __toml_lexer_new(__toml_lexer_t *lexer, string_view_t file);
PRIVATE void __toml_lexer_read_char(__toml_lexer_t *lexer);
PRIVATE void __toml_lexer_skip_whitespace(__toml_lexer_t *lexer);
PRIVATE int32_t __toml_lexer_next_token(__toml_lexer_t *lexer, __toml_token_t *t);
PRIVATE void  __toml_lexer_read_name(__toml_lexer_t *lexer, string_view_t *name);

PRIVATE int32_t __toml_parser_parse_tablename(toml_parser_t *p, __toml_lexer_t *, void *ctx, toml_parser_on_table on_table);
PRIVATE int32_t __toml_parser_emit_diagnostic(toml_parser_t *p, __toml_parser_diagnostic_t diagnostic);

#define TOML_PARSER_STOP 0xff

int32_t toml_parser_parse(toml_parser_t *parser, void *ctx, toml_parser_on_key_value_pair on_key_value_pair, toml_parser_on_table on_table)
{
  __toml_lexer_t l = {0};
  __toml_lexer_new(&l, parser->file);
  __toml_token_t token = {.value = string_view_empty(), .type = __TOML_TOKEN_TYPE_NONE };

  while (1)
  {
    TRY(__toml_lexer_next_token(&l, &token));     
    if (token.type == __TOML_TOKEN_TYPE_EOF)
    {
      break;
    }

    switch (token.type)
    {
    case __TOML_TOKEN_TYPE_LBRACKET:
    {
      int32_t err = __toml_parser_parse_tablename(parser, &l, ctx, on_table);
      if (err == TOML_PARSER_STOP)
      {
        return 0;
      }
      else if (err != 0)
      {
        return err;
      }
    }; break;
    case __TOML_TOKEN_TYPE_IDENTIFIER:
    {
      // TODO: we still don't handle key-value pairs, it would be used here...
      UNUSED(on_key_value_pair);
    }; break;
    case __TOML_TOKEN_TYPE_RBRACKET:
    {
      // TODO: show failure properly, keys can't start with a ']' and
      // this token should only appear in the context of a table, so it should already have been parsed
    }; break;
    case __TOML_TOKEN_TYPE_NONE:
    {
      // TODO: handle the case for unknown token
    }; break;
    case __TOML_TOKEN_TYPE_COUNT:
    case __TOML_TOKEN_TYPE_EOF:
    {
      // impossible
    }; break;
    }
  }
  return 0;
}

int32_t __toml_parser_parse_tablename(toml_parser_t *p, __toml_lexer_t *lexer, void *ctx, toml_parser_on_table on_table)
{
  __toml_token_t token = {0};
  TRY(__toml_lexer_next_token(lexer, &token));
  if (token.type != __TOML_TOKEN_TYPE_IDENTIFIER)
  {
    TRY(__toml_parser_emit_diagnostic(p, (__toml_parser_diagnostic_t){
                                      .line=token.line,
                                      .column=token.column,
                                      .error_message = string_view_from_cstr("error: broken tablename, an identifier should follow the opening bracket"),
                                    }));
    return TOML_PARSER_ERROR;
  }

  // tablename is token.value
  toml_parser_state_t s = on_table(ctx, token.value);
  if (s == TOML_PARSER_STATE_DONE)
  {
    return TOML_PARSER_STOP;
  }

  TRY(__toml_lexer_next_token(lexer, &token));
  if (token.type != __TOML_TOKEN_TYPE_RBRACKET)
  {
    UNUSED(p);
    // TODO: implement error handling
    return TOML_PARSER_ERROR;
  }
  return 0;
}

void __toml_lexer_new(__toml_lexer_t *lexer, string_view_t file)
{
  lexer->input = file;
  lexer->position = 0;
  lexer->read_position = 0;
  lexer->ch = 0;
  lexer->column = 1;
  lexer->line = 1;
  __toml_lexer_read_char(lexer);
}

void __toml_lexer_read_char(__toml_lexer_t *lexer)
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

int32_t __toml_lexer_next_token(__toml_lexer_t *lexer, __toml_token_t *tok)
{
  tok->type = __TOML_TOKEN_TYPE_NONE;
  tok->value = string_view_empty();

  __toml_lexer_skip_whitespace(lexer);

  tok->line = lexer->line;
  tok->column = lexer->column;

  switch (lexer->ch)
  {
  case '[':
  {
    tok->type  = __TOML_TOKEN_TYPE_LBRACKET;
    tok->value = string_view_from_cstr("[");
  }; break; 
  case ']':
  {
    tok->type  = __TOML_TOKEN_TYPE_RBRACKET;
    tok->value = string_view_from_cstr("]");
  }; break; 
  case 0:
  {
    tok->type = __TOML_TOKEN_TYPE_EOF;
    tok->value = string_view_empty();
  }; break;
  default:
  {
    if (__is_letter(lexer->ch))
    {
      __toml_lexer_read_name(lexer, &tok->value);
      tok->type = __TOML_TOKEN_TYPE_IDENTIFIER;
      return 0;
    }
  }; break;
  }

  __toml_lexer_read_char(lexer);
  return 0;
}

void  __toml_lexer_read_name(__toml_lexer_t *lexer, string_view_t *name)
{
  size_t pos = lexer->position;

  while (__is_letter(lexer->ch))
  {
    __toml_lexer_read_char(lexer);
  }

  name->addr = lexer->input.addr + pos;
  name->length = lexer->position - pos;
}

void __toml_lexer_skip_whitespace(__toml_lexer_t *lexer)
{
  while (lexer->ch == ' ' || lexer->ch == '\t' || lexer->ch == '\n' || lexer->ch == '\r')
  {
    __toml_lexer_read_char(lexer);
  }
}

uint8_t __is_letter(char c)
{
  return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z');
}

PRIVATE int32_t __toml_parser_emit_diagnostic(toml_parser_t *p, __toml_parser_diagnostic_t diagnostic)
{
  if (p->__diagnostics_len >= __TOML_PARSER_MAX_ERRORS)
  {
    return TOML_ERR_TOO_MANY_DIAGNOSTICS;
  }

  p->__diagnostics[p->__diagnostics_len++] = diagnostic;
  return 0;
}

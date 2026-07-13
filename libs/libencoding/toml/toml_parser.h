#ifndef SKENE_TOML_PARSER_H
#define SKENE_TOML_PARSER_H

#include <common/common.h>
#include <libstrview/string_view.h>

#define __TOML_PARSER_MAX_ERRORS 32

typedef struct {
  string_view_t error_message;
  size_t line;
  size_t column;
} __toml_parser_diagnostic_t;

typedef struct {
  string_view_t file;
  size_t offset;

  __toml_parser_diagnostic_t __diagnostics[__TOML_PARSER_MAX_ERRORS];
  size_t __diagnostics_len;
} toml_parser_t;

typedef struct {
  enum {
    TOML_VALUE_TAG_STRING,    
  } tag;

  union {
    string_view_t str;
  } as;
} toml_value_t;

typedef enum
{
  TOML_PARSER_STATE_DONE,
  TOML_PARSER_STATE_KEEPGOING,
} toml_parser_state_t;

#define TOML_PARSER_ERROR 1
#define TOML_ERR_TOO_MANY_DIAGNOSTICS 2

typedef toml_parser_state_t (*toml_parser_on_key_value_pair)(void *ctx, string_view_t tablename, string_view_t key, toml_value_t value);
typedef toml_parser_state_t (*toml_parser_on_table)(void *ctx, string_view_t tablename);

void toml_parser_new(toml_parser_t *, string_view_t file);
int32_t toml_parser_parse(toml_parser_t *parser, void *ctx, toml_parser_on_key_value_pair on_key_value_pair, toml_parser_on_table on_table);

#endif

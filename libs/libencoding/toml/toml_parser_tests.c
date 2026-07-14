#include <libencoding/toml/toml_parser.h>
#include <libstrview/string_view.h>
#include <libarena/arena.h>
#include <libhashtable/swisstables.h>

#include <libtest/test.h>

PRIVATE void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void);
PRIVATE void __toml_parser_should_be_able_to_parse_tables(void);
PRIVATE void __toml_parser_should_identify_errors_when_parsing_tables(void);
PRIVATE void __toml_parser_should_be_able_to_parse_key_value_pairs(void);
PRIVATE void __toml_parser_should_identify_errors_when_parsing_key_value_pairs(void);

int main(void)
{
  TEST_START(34);

  __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser();
  __toml_parser_should_be_able_to_parse_tables();
  __toml_parser_should_identify_errors_when_parsing_tables();
  __toml_parser_should_be_able_to_parse_key_value_pairs();
  __toml_parser_should_identify_errors_when_parsing_key_value_pairs();

  TEST_FINISH();
  return 0;
}

PRIVATE void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("Hello, world!");
  toml_parser_new(&p, file);

  ASSERT_BOOL(string_view_equals(file, p.file), "should assign the file to use the provided string view");
  ASSERT_BOOL(p.__diagnostics_len == 0, "the parser should have no diagnostics at startup");
}

#define MAX_TABLENAMES 32

typedef struct {
  arena_t arena;

  string_view_t tablenames[MAX_TABLENAMES];  
  size_t num_tablenames;

  sw_table_t hmap[MAX_TABLENAMES];
} toml_parsed_file;

PRIVATE toml_parser_state_t __toml_parser_on_table(void *ctx, string_view_t tablename)
{
  toml_parsed_file *f = (toml_parsed_file*)ctx;
  ASSERT_BOOL(f->num_tablenames < MAX_TABLENAMES, "we should never have a test that has over than 32 tables");
  if (f->num_tablenames >= MAX_TABLENAMES)
  {
    // we did smth dumb
    return TOML_PARSER_STATE_DONE;
  }

  f->tablenames[f->num_tablenames++] = tablename;

  return TOML_PARSER_STATE_KEEPGOING;
}

PRIVATE void __toml_parser_should_be_able_to_parse_tables(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("[tabledata]");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};
  int32_t err = toml_parser_parse(&p, (void *)&context, NULL, __toml_parser_on_table);
  ASSERT_INT_EQ(0, err, "there should be no errors on a valid TOML");
  ASSERT_INT_EQ(1, context.num_tablenames, "we should parse properly a single-tabled TOML");
  ASSERT_BOOL(string_view_equals(context.tablenames[0], string_view_from_cstr("tabledata")), "it should parse the tablename correctly");
}

PRIVATE void __toml_parser_should_identify_missing_tablename(void);
PRIVATE void __toml_parser_should_identify_missing_closing_bracket(void);
PRIVATE void __toml_parser_should_identify_token_starting_with_a_closing_bracket(void);

PRIVATE void __toml_parser_should_identify_errors_when_parsing_tables(void)
{
  __toml_parser_should_identify_missing_tablename();
  __toml_parser_should_identify_missing_closing_bracket();
  __toml_parser_should_identify_token_starting_with_a_closing_bracket();
}

PRIVATE void __toml_parser_should_identify_missing_tablename(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("[");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};
  int32_t err = toml_parser_parse(&p, (void *)&context, NULL, __toml_parser_on_table);

  ASSERT_INT_EQ(TOML_PARSER_ERROR, err, "there should be errors when parsing an invalid TOML");
  ASSERT_INT_EQ(1, p.__diagnostics_len, "there should be one diagnostic after parsing it");

  __toml_parser_diagnostic_t diagnostic = p.__diagnostics[0];

  ASSERT_BOOL(string_view_equals(diagnostic.error_message, string_view_from_cstr("error: broken tablename, an identifier should follow the opening bracket")), "the error message should match");
  ASSERT_INT_EQ(1, diagnostic.line, "it should correctly track the line of each diagnostic");
  ASSERT_INT_EQ(2, diagnostic.column, "it should correctly track the column of each diagnostic");
}

PRIVATE void __toml_parser_should_identify_missing_closing_bracket(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("[tabledata");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};
  int32_t err = toml_parser_parse(&p, (void *)&context, NULL, __toml_parser_on_table);

  ASSERT_INT_EQ(TOML_PARSER_ERROR, err, "there should be errors when parsing an invalid, missing closing bracket tablename TOML");
  ASSERT_INT_EQ(1, p.__diagnostics_len, "there should be one diagnostic after parsing it");

  __toml_parser_diagnostic_t diagnostic = p.__diagnostics[0];

  ASSERT_BOOL(string_view_equals(diagnostic.error_message, string_view_from_cstr("error: broken tablename, a closing bracket should follow the tablename")), "the error message should match");
  ASSERT_INT_EQ(1, diagnostic.line, "it should correctly track the line of each diagnostic");
  ASSERT_INT_EQ(11, diagnostic.column, "it should correctly track the column of each diagnostic");
}

PRIVATE void __toml_parser_should_identify_token_starting_with_a_closing_bracket(void)
{
  
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("]");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};
  int32_t err = toml_parser_parse(&p, (void *)&context, NULL, __toml_parser_on_table);

  ASSERT_INT_EQ(TOML_PARSER_ERROR, err, "there should be errors when parsing an invalid, tablename-starting-with-bracket TOML");
  ASSERT_INT_EQ(1, p.__diagnostics_len, "there should be one diagnostic after parsing it");

  __toml_parser_diagnostic_t diagnostic = p.__diagnostics[0];

  ASSERT_BOOL(string_view_equals(diagnostic.error_message, string_view_from_cstr("error: no TOML token starts with a ']'.")), "the error message should match");
  ASSERT_INT_EQ(1, diagnostic.line, "it should correctly track the line of each diagnostic");
  ASSERT_INT_EQ(1, diagnostic.column, "it should correctly track the column of each diagnostic");
}

toml_parser_state_t __toml_parser_on_key_value(void *ctx, string_view_t tablename, string_view_t key, toml_value_t value)
{
  toml_parsed_file *f = (toml_parsed_file*)ctx;
  ASSERT_BOOL(f->num_tablenames < MAX_TABLENAMES, "we should never have a test that has over than 32 tables");

  size_t idx = f->num_tablenames;
  f->tablenames[f->num_tablenames++] = tablename;

  int32_t err = sw_table_init(&f->hmap[idx], &f->arena, 8);
  ASSERT_INT_EQ(0, err, "there should no be errors when allocating a hashmap");

  toml_value_t *p = arena_new(&f->arena, toml_value_t);

  p->tag = value.tag;
  p->as = value.as;

  err = sw_table_insert(&f->hmap[idx], key, (void*)p);
  ASSERT_INT_EQ(0, err, "there should no be errors when inserting into the hashmap");

  return TOML_PARSER_STATE_KEEPGOING;
}

#define __BUFF_LEN 4096

PRIVATE void __toml_parser_should_be_able_to_parse_key_value_pairs(void)
{
  PRIVATE char __buff[__BUFF_LEN];
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("[tablename]\nkey=value");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};

  arena_new_with_underlying_buffer(&context.arena, __buff, __BUFF_LEN);

  int32_t err = toml_parser_parse(&p, (void *)&context, __toml_parser_on_key_value, NULL);
  ASSERT_INT_EQ(0, err, "there should be no errors when parsing an valid TOML");
  ASSERT_INT_EQ(0, p.__diagnostics_len, "there should be no diagnostics after parsing it");
  ASSERT_INT_EQ(1, context.num_tablenames, "we should parse one table name");

  void *result = NULL;
  int32_t found = sw_table_find(&context.hmap[0], string_view_from_cstr("key"), &result);
  ASSERT_INT_EQ(1, found, "should be able to find the key value pair");
  ASSERT_BOOL(result != NULL, "should be able to find the key value pair");

  toml_value_t *v = (toml_value_t *)result;
  ASSERT_INT_EQ(v->tag, TOML_VALUE_TAG_STRING, "the field should be of tag string");
  ASSERT_BOOL(string_view_equals(v->as.str, string_view_from_cstr("value")), "the value of the field should be 'value'");
}

PRIVATE void __toml_parser_should_fail_to_parse_malformed_value(void);

PRIVATE void __toml_parser_should_identify_errors_when_parsing_key_value_pairs(void)
{
  __toml_parser_should_fail_to_parse_malformed_value();
}

PRIVATE void __toml_parser_should_fail_to_parse_malformed_value(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("[tablename]\nkey==");
  toml_parser_new(&p, file);

  toml_parsed_file context = {0};

  int32_t err = toml_parser_parse(&p, (void *)&context, __toml_parser_on_key_value, NULL);
  ASSERT_INT_EQ(TOML_PARSER_ERROR, err, "there should errors when parsing TOML with a key that has '==' instead of '='");
  ASSERT_INT_EQ(1, p.__diagnostics_len, "there should be diagnostics after parsing it");
}

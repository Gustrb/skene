#include <libencoding/toml/toml_parser.h>
#include <libstrview/string_view.h>

#include <libtest/test.h>

PRIVATE void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void);
PRIVATE void __toml_parser_should_be_able_to_parse_tables(void);
PRIVATE void __toml_parser_should_identify_errors_when_parsing_tables(void);

int main(void)
{
  TEST_START(23);

  __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser();
  __toml_parser_should_be_able_to_parse_tables();
  __toml_parser_should_identify_errors_when_parsing_tables();

  TEST_FINISH();
  return 0;
}

PRIVATE void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("Hello, world!");
  toml_parser_new(&p, file);

  ASSERT_BOOL(string_view_equals(file, p.file), "should assign the file to use the provided string view");
  ASSERT_BOOL(p.offset == 0, "the parser offset should start at 0");
  ASSERT_BOOL(p.__diagnostics_len == 0, "the parser should have no diagnostics at startup");
}

#define MAX_TABLENAMES 32

typedef struct {
  string_view_t tablenames[MAX_TABLENAMES];  
  size_t num_tablenames;
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

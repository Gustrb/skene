#include <libencoding/toml/toml_parser.h>
#include <libstrview/string_view.h>

#include <libtest/test.h>

void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void);
void __toml_parser_should_be_able_to_parse_tables(void);

int main(void)
{
  TEST_START(6);

  __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser();
  __toml_parser_should_be_able_to_parse_tables();

  TEST_FINISH();
  return 0;
}

void __toml_parser_new_should_be_able_to_instantiate_a_new_toml_parser(void)
{
  toml_parser_t p = {0};
  string_view_t file = string_view_from_cstr("Hello, world!");
  toml_parser_new(&p, file);

  ASSERT_BOOL(string_view_equals(file, p.file), "should assign the file to use the provided string view");
  ASSERT_BOOL(p.offset == 0, "the parser offset should start at 0");
}

#define MAX_TABLENAMES 32

typedef struct {
  string_view_t tablenames[MAX_TABLENAMES];  
  size_t num_tablenames;
} toml_parsed_file;

toml_parser_state_t __toml_parser_on_table(void *ctx, string_view_t tablename)
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

void __toml_parser_should_be_able_to_parse_tables(void)
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

#include <string.h>

#include <libstrview/string_view.h>

PUBLIC uint8_t string_view_equals(string_view_t a, string_view_t b)
{
  if (a.length != b.length)
  {
    return 0;
  }

  if (a.addr == b.addr)
  {
    return 1;
  }

  ASSUME(a.length == b.length);

  // TODO: SIMD??
  for (size_t i = 0; i < a.length; ++i)
  {
    if (a.addr[i] != b.addr[i])
    {
      return 0;
    }
  }

  return 1;
}

PUBLIC string_view_t string_view_from_cstr(const char *a)
{
  size_t len = strlen(a);
  return (string_view_t){.length=len, .addr=a};
}

PUBLIC string_view_split_iter_t string_view_split(string_view_t a, char sep)
{
  return (string_view_split_iter_t){.done=0, .rest=a, .separator=sep};
}

PUBLIC string_view_t string_view_split_iter_next(string_view_split_iter_t *svsi)
{
  string_view_t sv = {0};
  if (svsi->done)
  {
    return sv;
  }

  const char *start = svsi->rest.addr;
  const char *end = svsi->rest.addr + svsi->rest.length;

  const char *aux = start;
  while (aux < end && *aux != svsi->separator) aux++;

  sv.addr = start;
  sv.length = aux-start;

  if (aux < end)
  {
    svsi->rest.addr = aux + 1;
    svsi->rest.length = end - (aux + 1);
  }
  else
  {
    svsi->done = 1;
  }

  return sv;
}

PUBLIC uint8_t string_view_starts_with(string_view_t a, string_view_t prefix)
{
  if (prefix.length > a.length)
  {
    return 0;
  }

  ASSUME(a.length >= prefix.length);

  for (size_t i = 0; i < prefix.length; ++i)
  {
    if (a.addr[i] != prefix.addr[i])
    {
      return 0;
    }
  }

	return 1;
}

PUBLIC uint8_t string_view_starts_with_cstr(string_view_t a, const char *b)
{
  return string_view_starts_with(a, string_view_from_cstr(b));
}

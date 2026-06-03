#include <sys/inotify.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>

#include "common/common.h"
#include "libstrbuilder/strbuilder.h"

#define NUM_CLI_COMMANDS 6

#define CLI_COMMAND_INPUT_FILE 1
#define CLI_COMMAND_OUTPUT_FILE 5

#define APPEND_CSTR_ERROR(builder, str) \
  do \
  { \
    int32_t ___err = string_builder_append_cstr(builder, str);\
    if (___err != 0) return 1;\
  } while(0);

static const char *cli_commands[NUM_CLI_COMMANDS] = {
  "gcc",
  NULL,
  "-fverbose-asm",
  "-S",
  "-o",
  NULL,
};

#define NAME_MAX 255
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#define BUFF_LEN (10 * (sizeof(struct inotify_event) + NAME_MAX + 1))

PRIVATE int32_t recompile(const char *input, const char *output, string_builder_t *sb);
PRIVATE void print_file(const char *path);

int main(int argc, const char **argv)
{
  if (argc != 2)
  {
    fprintf(stderr, "usage: %s <file.c>\n", argv[0]);
    return 1;
  }

  const char *file_to_compile_path = argv[1];
  // TODO: generate random file path
  const char *output = "output.S";

  // inotify watches inodes, not names. Editors like vim save by writing a
  // temp file and renaming it over the original, which swaps the inode and
  // makes a watch on the file itself go stale. So we watch the parent
  // directory (whose inode is stable) and filter events by file name.
  const char *slash = strrchr(file_to_compile_path, '/');

  const char *filename;
  char dir[PATH_MAX];

  if (slash == NULL)
  {
    // no directory component, watch the cwd
    dir[0] = '.';
    dir[1] = '\0';
    filename = file_to_compile_path;
  }
  else
  {
    size_t dirlen = (size_t)(slash - file_to_compile_path);
    if (dirlen == 0) dirlen = 1; // "/foo.c" -> dir is "/"
    if (dirlen >= PATH_MAX-1)
    {
      return 1;
    }

    memcpy(dir, file_to_compile_path, dirlen);
    dir[dirlen] = '\0';
    filename = slash + 1;
  }

  // We watch the parent directory, which can exist even when the file
  // doesn't — check the file itself up front so a bad path fails fast
  // instead of waiting forever.
  if (access(file_to_compile_path, R_OK) != 0)
  {
    perror(file_to_compile_path);
    return 1;
  }

  string_builder_t builder = {0};
  int32_t err = string_builder_new(&builder);
  if (err != 0)
  {
    return 1;
  }

  char buff[BUFF_LEN] = {0};
  int inotify_fd = inotify_init();
  if (inotify_fd < 0)
  {
    string_builder_destroy(&builder);
    return 1;
  }

  // IN_CLOSE_WRITE catches in-place saves; IN_MOVED_TO catches the
  // rename-over-original that editors do on atomic save.
  int wd = inotify_add_watch(inotify_fd, dir, IN_CLOSE_WRITE | IN_MOVED_TO);
  if (wd < 0)
  {
    close(inotify_fd);
    string_builder_destroy(&builder);
    return 1;
  }

  while (1)
  {
    int n = read(inotify_fd, buff, BUFF_LEN);
    char *p = buff;

    while (p < buff + n)
    {
      struct inotify_event *event = (struct inotify_event *)p;
      if (event->len && strcmp(event->name, filename) == 0)
      {
        err = recompile(file_to_compile_path, output, &builder);
        builder.length = 0;

        if (err != 0)
        {
          // A compile failure shouldn't kill the watcher; report and keep going.
          fprintf(stderr, "compile failed (exit %d)\n", err);
        }
        else
        {
          print_file(output);
        }
      }

      p += sizeof(struct inotify_event) + event->len;
    }
  }

  close(inotify_fd);
  string_builder_destroy(&builder);

  return 0;
}

PRIVATE void print_file(const char *path)
{
  printf("\033[2J");
  printf("\033[H");

  FILE *f = fopen(path, "r");
  if (f == NULL)
  {
    perror("fopen");
    return;
  }

  char buf[4096];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
  {
    fwrite(buf, 1, n, stdout);
  }

  fflush(stdout);
  fclose(f);
}

PRIVATE int32_t recompile(const char *input, const char *output, string_builder_t *sb)
{
  int32_t err = 0;

  for (size_t i = 0; i < NUM_CLI_COMMANDS; i++)
  {
    if (i == CLI_COMMAND_INPUT_FILE)
    {
      APPEND_CSTR_ERROR(sb, input);
      APPEND_CSTR_ERROR(sb, " ");
    }
    else if (i == CLI_COMMAND_OUTPUT_FILE)
    {
      APPEND_CSTR_ERROR(sb, output);
      APPEND_CSTR_ERROR(sb, " ");
    }
    else
    {
      APPEND_CSTR_ERROR(sb, cli_commands[i]);
      APPEND_CSTR_ERROR(sb, " ");
    }
  }

  const char *command;
  err = string_builder_into_owned_cstr(sb, &command);
  if (err != 0)
  {
    return 1;
  }

  int err_code = system(command);
  free((void *)command);
  if (err_code != 0)
  {
    return err_code;
  }

  return 0;
}

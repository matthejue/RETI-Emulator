#include "../include/special_opts.h"
#include "../include/debug.h"
#include "../include/error.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *out_file = NULL;
FILE *err_file = NULL;

uint32_t *extract_input_from_comment(const char *line, uint8_t *len) {
  const char *prefix;
  if (strncmp(line, "# input:", strlen("# input:")) == 0) {
    prefix = "# input:";
  } else if (strncmp(line, "#input:", strlen("#input:")) == 0) {
    prefix = "#input:";
  } else {
    return NULL;
  }

  const char *ptr = line + strlen(prefix);
  uint32_t *ar = NULL;
  uint8_t count = 0;

  while (*ptr) {
    while (isspace((unsigned char)*ptr)) {
      ptr++;
    }

    if (*ptr == '\0') {
      break;
    }

    ar = realloc(ar, (count + 1) * sizeof(uint32_t));
    if (isdigit((char)*ptr) || *ptr == '-') {
      uint8_t *original_ptr = (uint8_t *)ptr;
      uint64_t num = strtol(ptr, (char **)&ptr, 10);
      if ((int64_t)num < INT32_MIN || (int64_t)num > INT32_MAX) {
        uint8_t idx_of_newline = strcspn((char *)original_ptr, "\n");
        original_ptr[idx_of_newline] = '\0';
        display_error_message(
            "InputError",
            "Number must be between -2147483648 and 2147483647, got \"%s\"",
            (char *)original_ptr, Pntr);

        exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
      }
      ar[count++] = (uint32_t)num;
    } else {
      ar[count++] = (uint8_t)*ptr++;
    }
  }

  *len = count;
  return ar;
}

bool first_line_over = false;

uint32_t *extract_comment_metadata(const char *prgrm_path, uint8_t *len) {
  error_context.filename = prgrm_path;
  FILE *file = fopen(prgrm_path, "r");
  if (file == NULL) {
    fprintf(stderr, "Error: Couldn't open file\n");
    exit(EXIT_FAILURE);
  }

  char line[256];
  uint32_t *result = NULL;
  *len = 0;

  while (fgets(line, sizeof(line), file)) {
    if (line[0] == '\n') {
      continue;
    }
    if (first_line_over) {
      error_context.code_current = line;
    } else {
      error_context.code_begin = line;
      error_context.code_current = line;
      first_line_over = true;
    }

    if (line[0] == '#') {
      uint32_t *extract_ar = extract_input_from_comment(line, len);
      if (extract_ar) {
        result = extract_ar;
      }
    } else {
      break;
    }
  }

  fclose(file);
  return result;
}

void create_out_and_err_file() {
  char *file_path = strdup(sram_prgrm_path);
  char *ext = strrchr(file_path, '.');
  if (ext != NULL) {
    *ext = '\0';
  }

  char *out_file_path = proper_str_cat(file_path, ".output");
  out_file = fopen(out_file_path, "w");
  if (out_file == NULL) {
    fprintf(stderr, "Error: Can't open file\n");
    exit(EXIT_FAILURE);
  }

  char *err_file_path = proper_str_cat(file_path, ".error");
  err_file = fopen(err_file_path, "w");
  if (err_file == NULL) {
    fprintf(stderr, "Error: Can't open file\n");
    exit(EXIT_FAILURE);
  }
}

void adjust_print(bool is_stdout, const char *format,
                  const char *format_no_newline, ...) {
  va_list args;
  va_start(args, format_no_newline);

  if (test_mode) {
    if (is_stdout) {
      vfprintf(out_file, format_no_newline, args);
    } else {
      vfprintf(err_file, format, args);
    }
  } else {
    if (is_stdout) { // because of display_error_message in case test_mode is
                     // false
      if (format != NULL) {
        vfprintf(stdout, format, args);
      }
    } else {
      vfprintf(stderr, format, args);
    }
  }

  va_end(args);
}

void close_out_and_err_file() {
  fclose(out_file);
  fclose(err_file);
}

void finalize() {
  fin_reti();
  if (test_mode) {
    close_out_and_err_file();
  } else {
    fin_tui();
  }
}

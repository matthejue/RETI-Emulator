#include "../include/special_opts.h"
#include "../include/core_debug.h"
#include "../include/error.h"
#include "../include/parse/parse_args.h"
#include "../include/parse/parse_instrs.h"
#include "../include/reti.h"
#include "../include/source_debug.h"
#include "../include/terminal_view.h"
#include "../include/utils.h"
#include "../include/uart.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

FILE *out_file = NULL;
FILE *err_file = NULL;

uint8_t *extract_input_from_comment(const char *line, size_t *len) {
  const char *prefix;
  if (!strncmp(line, "# input:", strlen("# input:"))) {
    prefix = "# input:";
  } else if (!strncmp(line, "#input:", strlen("#input:"))) {
    prefix = "#input:";
  } else {
    return NULL;
  }

  const char *ptr = line + strlen(prefix);
  /* keep the first whitespace. Commenting out the automatic
     skip of a single space/tab after the prefix. */
  /* if (*ptr == ' ' || *ptr == '\t') {
    ptr++;
  } */

  *len = strcspn(ptr, "\n");
  uint8_t *input = malloc(*len + 2);
  memcpy(input, ptr, *len);
  *len = decode_uart_input_escapes(input, (uint16_t)*len);
  input[(*len)++] = '\n';
  input[*len] = '\0';
  return input;
}

bool first_line_over = false;

uint8_t *extract_comment_metadata(const char *prgrm_path, size_t *len) {
  error_context.filename = prgrm_path;
  FILE *file = fopen(prgrm_path, "r");
  if (file == NULL) {
    fprintf(stderr, "Error: Couldn't open file\n");
    exit(EXIT_FAILURE);
  }

  char line[256];
  uint8_t *result = NULL;
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
      uint8_t *extract_ar = extract_input_from_comment(line, len);
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
  stop_source_debugger();
  stop_terminal_viewer();
  close_terminal_output();
  fin_reti();
  free_program_comments();
  if (test_mode) {
    close_out_and_err_file();
  }
  if (debug_mode) {
    fin_tui();
  }
}

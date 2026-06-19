#include "../include/special_opts.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_extract_comment_metadata() {
  char *filename = "/tmp/extract_comment_metadata.reti";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    fprintf(stderr, "Error: File could not be opened\n");
    exit(EXIT_FAILURE);
  }
  fprintf(file, "# input: 72 ello 32 Wo 114 ld\n");
  fprintf(file, "JUMP 0\n");
  fclose(file);
  size_t len = 0;
  uint8_t *comment_metadata = extract_comment_metadata(filename, &len);
  const char *expected = " 72 ello 32 Wo 114 ld\n";
  assert(len == strlen(expected));
  assert(memcmp(comment_metadata, expected, len) == 0);
  free(comment_metadata);
}

void test_extract_comment_metadata_decimal_escapes() {
  char *filename = "/tmp/extract_comment_metadata_decimal_escapes.reti";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    fprintf(stderr, "Error: File could not be opened\n");
    exit(EXIT_FAILURE);
  }
  fprintf(file, "# input: A\\d066\\d000\\d255Z\n");
  fprintf(file, "JUMP 0\n");
  fclose(file);
  size_t len = 0;
  uint8_t *comment_metadata = extract_comment_metadata(filename, &len);
  const uint8_t expected[] = {' ', 'A', 66, 0, 255, 'Z', '\n'};
  assert(len == sizeof(expected));
  assert(memcmp(comment_metadata, expected, len) == 0);
  free(comment_metadata);
}

void test_common_escapes() {
  uint8_t input[] = "\\n\\t\\r\\0\\a\\b\\f\\v\\\\";
  uint16_t len = decode_uart_input_escapes(input, strlen((char *)input));
  const uint8_t expected[] = {'\n', '\t', '\r', '\0', '\a',
                              '\b', '\f', '\v', '\\'};
  assert(len == sizeof(expected));
  assert(memcmp(input, expected, len) == 0);
}

void test_decimal_escape_above_byte_range_stays_literal() {
  uint8_t input[] = "\\d255\\d256";
  uint16_t len = decode_uart_input_escapes(input, strlen((char *)input));
  const uint8_t expected[] = {255, '\\', 'd', '2', '5', '6'};
  assert(len == sizeof(expected));
  assert(memcmp(input, expected, len) == 0);
}

void test_format_uart_byte() {
  char buffer[6];
  assert(strcmp(format_uart_byte('\n', buffer), "\\n") == 0);
  assert(strcmp(format_uart_byte('\t', buffer), "\\t") == 0);
  assert(strcmp(format_uart_byte('\0', buffer), "\\0") == 0);
  assert(strcmp(format_uart_byte('\\', buffer), "\\\\") == 0);
  assert(strcmp(format_uart_byte('A', buffer), "A") == 0);
  assert(strcmp(format_uart_byte(1, buffer), "\\d001") == 0);
  assert(strcmp(format_uart_byte(255, buffer), "\\d255") == 0);
}

void test_uart_load_command_appends_file_to_input() {
  const char *filename = "uart_load_test.bin";
  const uint8_t file_content[] = {'A', '\0', 255, 'B', 'C', 'D', 'E', 'F'};
  const uint8_t expected_word_count[] = {0, 0, 0, 2};
  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    fprintf(stderr, "Error: File could not be opened\n");
    exit(EXIT_FAILURE);
  }
  fwrite(file_content, 1, sizeof(file_content), file);
  fclose(file);

  free(uart_input);
  uart_input = malloc(3);
  memcpy(uart_input, "xy", 2);
  uart_input[2] = '\0';
  input_len = 2;
  input_idx = 1;

  const char *ignored_output = "hello\n";
  for (size_t i = 0; i < strlen(ignored_output); i++) {
    uart_handle_sent_byte_for_load_command(ignored_output[i]);
  }
  assert(input_len == 2);

  const char *command = "load uart_load_test.bin\n";
  for (size_t i = 0; i < strlen(command); i++) {
    uart_handle_sent_byte_for_load_command(command[i]);
  }

  assert(input_len ==
         2 + sizeof(expected_word_count) + sizeof(file_content));
  assert(input_idx == 1);
  assert(uart_input[1] == 'y');
  assert(memcmp(uart_input + 2, expected_word_count,
                sizeof(expected_word_count)) == 0);
  assert(memcmp(uart_input + 2 + sizeof(expected_word_count), file_content,
                sizeof(file_content)) == 0);

  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;
  remove(filename);
}

int main() {
  test_extract_comment_metadata();
  test_extract_comment_metadata_decimal_escapes();
  test_common_escapes();
  test_decimal_escape_above_byte_range_stays_literal();
  test_format_uart_byte();
  test_uart_load_command_appends_file_to_input();
  return 0;
}

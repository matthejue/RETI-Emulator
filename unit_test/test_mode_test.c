#include "../include/special_opts.h"
#include "../include/uart.h"
#include "../include/parse/parse_args.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void send_uart_byte(uint8_t byte) {
  uart[0] = byte;
  uart[2] &= 0b11111110;
  update_uart();
}

static void send_uart_bytes(const uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    send_uart_byte(bytes[i]);
  }
}

static uint32_t read_uart_u32(size_t offset) {
  return ((uint32_t)uart_input[offset] << 24) |
         ((uint32_t)uart_input[offset + 1] << 16) |
         ((uint32_t)uart_input[offset + 2] << 8) | uart_input[offset + 3];
}

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

void test_escaped_uart_load_command_appends_file_to_input() {
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

  init_uart();
  max_waiting_instrs = 0;
  free(uart_input);
  uart_input = malloc(3);
  memcpy(uart_input, "xy", 2);
  uart_input[2] = '\0';
  input_len = 2;
  input_idx = 1;

  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  const char *ordinary_load = "load uart_load_test.bin\n";
  send_uart_bytes((const uint8_t *)ordinary_load, strlen(ordinary_load));
  fflush(stdout);
  rewind(stdout_capture);
  char ordinary_output[26] = {0};
  assert(fread(ordinary_output, 1, strlen(ordinary_load), stdout_capture) ==
         strlen(ordinary_load));
  assert(memcmp(ordinary_output, ordinary_load, strlen(ordinary_load)) == 0);
  assert(fgetc(stdout_capture) == EOF);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);
  assert(input_len == 2);

  const uint8_t command[] = "\x1bload uart_load_test.bin\x1b/";
  send_uart_bytes(command, sizeof(command) - 1);

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
  close_uart_output();
  free(uart);
  uart = NULL;
  remove(filename);
}

void test_escaped_uart_read_range_command_appends_only_requested_bytes() {
  const char *filename = "uart_read_range_test.bin";
  const uint8_t file_content[] = {'A', '\0', 255, 'B', 'C', 'D', 'E', 'F'};
  const uint8_t expected_file_size[] = {0, 0, 0, 8};
  const uint8_t expected_byte_count[] = {0, 0, 0, 3};
  FILE *file = fopen(filename, "wb");
  assert(file != NULL);
  assert(fwrite(file_content, 1, sizeof(file_content), file) ==
         sizeof(file_content));
  fclose(file);

  init_uart();
  max_waiting_instrs = 0;
  free(uart_input);
  uart_input = malloc(3);
  memcpy(uart_input, "xy", 2);
  uart_input[2] = '\0';
  input_len = 2;
  input_idx = 1;

  const uint8_t command[] =
      "\x1bread-range 2 3 uart_read_range_test.bin\x1b/";
  send_uart_bytes(command, sizeof(command) - 1);

  assert(input_len == 2 + sizeof(expected_byte_count) + 3);
  assert(input_idx == 1);
  assert(uart_input[1] == 'y');
  assert(memcmp(uart_input + 2, expected_byte_count,
                sizeof(expected_byte_count)) == 0);
  assert(memcmp(uart_input + 2 + sizeof(expected_byte_count),
                file_content + 2, 3) == 0);

  const size_t previous_input_len = input_len;
  const uint8_t zero_range_command[] =
      "\x1bread-range 0 0 uart_read_range_test.bin\x1b/";
  send_uart_bytes(zero_range_command, sizeof(zero_range_command) - 1);
  assert(input_len == previous_input_len + sizeof(uint32_t));
  const uint8_t zero_byte_count[] = {0, 0, 0, 0};
  assert(memcmp(uart_input + previous_input_len, zero_byte_count,
                sizeof(zero_byte_count)) == 0);

  const size_t before_short_range = input_len;
  const uint8_t short_range_command[] =
      "\x1bread-range 6 4 uart_read_range_test.bin\x1b/";
  const uint8_t short_byte_count[] = {0, 0, 0, 2};
  send_uart_bytes(short_range_command, sizeof(short_range_command) - 1);
  assert(input_len ==
         before_short_range + sizeof(short_byte_count) + 2);
  assert(memcmp(uart_input + before_short_range, short_byte_count,
                sizeof(short_byte_count)) == 0);
  assert(memcmp(uart_input + before_short_range + sizeof(short_byte_count),
                file_content + 6, 2) == 0);

  const size_t before_missing_range = input_len;
  const uint8_t missing_range_command[] =
      "\x1bread-range 0 1 uart_read_range_missing.bin\x1b/";
  const uint8_t missing_file[] = {255, 255, 255, 255};
  send_uart_bytes(missing_range_command, sizeof(missing_range_command) - 1);
  assert(input_len == before_missing_range + sizeof(missing_file));
  assert(memcmp(uart_input + before_missing_range, missing_file,
                sizeof(missing_file)) == 0);

  const size_t before_file_size = input_len;
  const uint8_t file_size_command[] =
      "\x1b" "file-size uart_read_range_test.bin\x1b/";
  send_uart_bytes(file_size_command, sizeof(file_size_command) - 1);
  assert(input_len == before_file_size + sizeof(expected_file_size));
  assert(memcmp(uart_input + before_file_size, expected_file_size,
                sizeof(expected_file_size)) == 0);

  const size_t before_missing_file = input_len;
  const uint8_t missing_file_command[] =
      "\x1b" "file-size uart_read_range_missing.bin\x1b/";
  send_uart_bytes(missing_file_command, sizeof(missing_file_command) - 1);
  assert(input_len == before_missing_file + sizeof(missing_file));
  assert(memcmp(uart_input + before_missing_file, missing_file,
                sizeof(missing_file)) == 0);

  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;
  close_uart_output();
  free(uart);
  uart = NULL;
  remove(filename);
}

void test_uart_completes_load_command_before_receive() {
  const char *filename = "uart_load_order_test.bin";
  const uint8_t file_content[] = {'A', 'B', 'C', 'D'};
  FILE *file = fopen(filename, "wb");
  if (file == NULL) {
    fprintf(stderr, "Error: File could not be opened\n");
    exit(EXIT_FAILURE);
  }
  fwrite(file_content, 1, sizeof(file_content), file);
  fclose(file);

  init_uart();
  max_waiting_instrs = 0;
  free(uart_input);
  uart_input = malloc(1);
  uart_input[0] = '\0';
  input_len = 0;
  input_idx = 0;

  const uint8_t command[] = "\x1bload uart_load_order_test.bin\x1b";
  send_uart_bytes(command, sizeof(command) - 1);

  max_waiting_instrs = 1;
  uart[0] = '/';
  uart[2] = 0;
  update_uart();
  assert(input_len == 0);
  assert(input_idx == 0);
  update_uart();

  assert(input_len == sizeof(uint32_t) + sizeof(file_content));
  assert(input_idx == 1);
  assert(receive_current_byte == 0);

  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;
  close_uart_output();
  free(uart);
  uart = NULL;
  remove(filename);
}

void test_uart_host_filesystem_commands() {
  char original_directory[4096];
  assert(getcwd(original_directory, sizeof(original_directory)) != NULL);
  char temporary_directory[] = "/tmp/uart_host_services_XXXXXX";
  assert(mkdtemp(temporary_directory) != NULL);

  char marker_path[4096];
  char renamed_path[4096];
  char touched_path[4096];
  snprintf(marker_path, sizeof(marker_path), "%s/marker.txt",
           temporary_directory);
  FILE *marker = fopen(marker_path, "wb");
  assert(marker != NULL);
  assert(fwrite("abc", 1, 3, marker) == 3);
  fclose(marker);

  init_uart();
  max_waiting_instrs = 0;
  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;

  const uint8_t pwd_command[] = "\x1bpwd\x1b/";
  send_uart_bytes(pwd_command, sizeof(pwd_command) - 1);
  uint32_t length = read_uart_u32(0);
  assert(length == strlen(original_directory));
  assert(memcmp(uart_input + 4, original_directory, length) == 0);
  size_t offset = 4 + length;

  char command[16384];
  snprintf(command, sizeof(command), "\x1b" "is-directory %s\x1b/",
           temporary_directory);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;
  char current_directory[4096];
  assert(getcwd(current_directory, sizeof(current_directory)) != NULL);
  assert(strcmp(current_directory, original_directory) == 0);

  snprintf(command, sizeof(command), "\x1b" "is-directory %s\x1b/",
           marker_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == UINT32_MAX);
  offset += 4;

  snprintf(command, sizeof(command), "\x1bmkdir %s/created\x1b/",
           temporary_directory);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;

  snprintf(command, sizeof(command), "\x1bls %s\x1b/",
           temporary_directory);
  send_uart_bytes((uint8_t *)command, strlen(command));
  length = read_uart_u32(offset);
  char *listing = malloc(length + 1);
  assert(listing != NULL);
  memcpy(listing, uart_input + offset + 4, length);
  listing[length] = '\0';
  assert(strstr(listing, "d .\n") != NULL);
  assert(strstr(listing, "d ..\n") != NULL);
  assert(strstr(listing, "- marker.txt\n") != NULL);
  assert(strstr(listing, "d created\n") != NULL);
  free(listing);
  offset += 4 + length;

  snprintf(renamed_path, sizeof(renamed_path), "%s/renamed.txt",
           temporary_directory);
  snprintf(command, sizeof(command), "\x1bmove %s\n%s\x1b/", marker_path,
           renamed_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;
  assert(access(marker_path, F_OK) != 0);
  assert(access(renamed_path, F_OK) == 0);

  snprintf(touched_path, sizeof(touched_path), "%s/touched.txt",
           temporary_directory);
  snprintf(command, sizeof(command), "\x1btouch %s\x1b/", touched_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;
  assert(access(touched_path, F_OK) == 0);

  snprintf(command, sizeof(command), "\x1bunlink %s\x1b/", renamed_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;

  snprintf(command, sizeof(command), "\x1bunlink %s\x1b/", touched_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;

  char created_path[4096];
  snprintf(created_path, sizeof(created_path), "%s/created",
           temporary_directory);
  snprintf(command, sizeof(command), "\x1brmdir %s\x1b/", created_path);
  send_uart_bytes((uint8_t *)command, strlen(command));
  assert(read_uart_u32(offset) == 0);
  offset += 4;

  const uint8_t removed_host_command[] = "\x1b!pwd\x1b/";
  send_uart_bytes(removed_host_command, sizeof(removed_host_command) - 1);
  assert(input_len == offset);

  assert(rmdir(temporary_directory) == 0);

  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;
  close_uart_output();
  free(uart);
  uart = NULL;
}

int main() {
  test_extract_comment_metadata();
  test_extract_comment_metadata_decimal_escapes();
  test_common_escapes();
  test_decimal_escape_above_byte_range_stays_literal();
  test_format_uart_byte();
  test_escaped_uart_load_command_appends_file_to_input();
  test_escaped_uart_read_range_command_appends_only_requested_bytes();
  test_uart_completes_load_command_before_receive();
  test_uart_host_filesystem_commands();
  return 0;
}

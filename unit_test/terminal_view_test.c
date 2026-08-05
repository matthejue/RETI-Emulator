#include "../include/parse/parse_args.h"
#include "../include/terminal_view.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *TERMINAL_OUTPUT_PATH =
    "/tmp/.reti_emulaor/terminal_output.bin";

static void start_uart(void) {
  init_uart();
  max_waiting_instrs = 0;
}

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

static void stop_uart(void) {
  close_uart_output();
  free(uart);
  uart = NULL;
}

static size_t read_file(const char *path, uint8_t *buffer, size_t capacity) {
  FILE *output = fopen(path, "rb");
  assert(output != NULL);
  size_t len = fread(buffer, 1, capacity, output);
  assert(fgetc(output) == EOF);
  fclose(output);
  return len;
}

static void test_debug_uart_output_is_captured_and_replayed(void) {
  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  debug_mode = true;
  assert(init_terminal_output());
  start_uart();
  send_uart_byte('A');
  stop_uart();
  close_terminal_output();
  fflush(stdout);
  assert(ftell(stdout_capture) == 0);
  assert(replay_terminal_output());
  fflush(stdout);
  rewind(stdout_capture);
  assert(fgetc(stdout_capture) == 'A');
  assert(fgetc(stdout_capture) == EOF);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);

  uint8_t output[1];
  assert(read_file(TERMINAL_OUTPUT_PATH, output, sizeof(output)) == 1);
  assert(output[0] == 'A');
}

static void test_non_debug_uart_output_uses_stdout(void) {
  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  debug_mode = false;
  start_uart();
  send_uart_byte('B');
  stop_uart();
  fflush(stdout);
  rewind(stdout_capture);
  assert(fgetc(stdout_capture) == 'B');
  assert(fgetc(stdout_capture) == EOF);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);
}

static void test_uart_controls_are_hidden_from_debug_terminal(void) {
  debug_mode = true;
  assert(init_terminal_output());
  start_uart();

  const uint8_t output[] = "A\x1bwrite stdout\x1b/B";
  send_uart_bytes(output, sizeof(output) - 1);

  stop_uart();
  close_terminal_output();
  uint8_t captured[2];
  assert(read_file(TERMINAL_OUTPUT_PATH, captured, sizeof(captured)) == 2);
  assert(memcmp(captured, "AB", sizeof(captured)) == 0);
}

static void test_uart_output_can_be_redirected_to_a_file(void) {
  const char *path = "/tmp/reti_uart_output_test.bin";
  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  debug_mode = false;
  start_uart();
  const uint8_t output[] =
      "\x1bwrite /tmp/reti_uart_output_test.bin\x1b/"
      "file\x1bstdout\x1b/still\x1bwrite stdout\x1b/terminal";
  send_uart_bytes(output, sizeof(output) - 1);
  stop_uart();

  fflush(stdout);
  rewind(stdout_capture);
  char stdout_output[9] = {0};
  assert(fread(stdout_output, 1, 8, stdout_capture) == 8);
  assert(memcmp(stdout_output, "terminal", 8) == 0);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);

  uint8_t file_output[9];
  assert(read_file(path, file_output, sizeof(file_output)) == 9);
  assert(memcmp(file_output, "filestill", sizeof(file_output)) == 0);
  remove(path);
}

static void test_uart_output_can_be_redirected_to_stderr(void) {
  FILE *stdout_capture = tmpfile();
  FILE *stderr_capture = tmpfile();
  assert(stdout_capture != NULL);
  assert(stderr_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  int saved_stderr = dup(STDERR_FILENO);
  assert(saved_stdout >= 0);
  assert(saved_stderr >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);
  assert(dup2(fileno(stderr_capture), STDERR_FILENO) >= 0);

  debug_mode = false;
  start_uart();
  const uint8_t output[] =
      "\x1bwrite stderr\x1b/E\x1bwrite stdout\x1b/O";
  send_uart_bytes(output, sizeof(output) - 1);
  stop_uart();

  fflush(stdout);
  fflush(stderr);
  rewind(stdout_capture);
  rewind(stderr_capture);
  assert(fgetc(stdout_capture) == 'O');
  assert(fgetc(stdout_capture) == EOF);
  assert(fgetc(stderr_capture) == 'E');
  assert(fgetc(stderr_capture) == EOF);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  assert(dup2(saved_stderr, STDERR_FILENO) >= 0);
  close(saved_stdout);
  close(saved_stderr);
  fclose(stdout_capture);
  fclose(stderr_capture);
}

static void test_uart_terminal_command_runs_in_emulator_directory(void) {
  const char *path = "uart_terminal_command_test.txt";
  debug_mode = false;
  start_uart();
  const uint8_t command[] =
      "\x1b!pwd > uart_terminal_command_test.txt\x1b/";
  send_uart_bytes(command, sizeof(command) - 1);
  stop_uart();

  char expected[4096];
  assert(getcwd(expected, sizeof(expected)) != NULL);
  strcat(expected, "\n");
  uint8_t actual[4096];
  size_t len = read_file(path, actual, sizeof(actual));
  assert(len == strlen(expected));
  assert(memcmp(actual, expected, len) == 0);
  remove(path);
}

int main(void) {
  peripherals_dir = "/tmp";
  test_debug_uart_output_is_captured_and_replayed();
  test_non_debug_uart_output_uses_stdout();
  test_uart_controls_are_hidden_from_debug_terminal();
  test_uart_output_can_be_redirected_to_a_file();
  test_uart_output_can_be_redirected_to_stderr();
  test_uart_terminal_command_runs_in_emulator_directory();
  return 0;
}

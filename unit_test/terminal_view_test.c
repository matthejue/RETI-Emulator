#include "../include/parse/parse_args.h"
#include "../include/terminal_view.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static const char *TERMINAL_OUTPUT_PATH =
    "/tmp/reti_emulator/terminal_output.bin";

static void send_uart_byte(uint8_t byte) {
  init_uart();
  max_waiting_instrs = 0;
  uart[0] = byte;
  uart[2] = 0b00000010;
  update_uart();
  free(uart);
  uart = NULL;
}

static uint8_t read_captured_terminal_byte(void) {
  FILE *output = fopen(TERMINAL_OUTPUT_PATH, "rb");
  assert(output != NULL);
  int byte = fgetc(output);
  assert(byte != EOF);
  assert(fgetc(output) == EOF);
  fclose(output);
  return (uint8_t)byte;
}

static void test_debug_uart_output_is_captured(void) {
  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  debug_mode = true;
  assert(init_terminal_output());
  send_uart_byte('A');
  close_terminal_output();
  fflush(stdout);
  assert(ftell(stdout_capture) == 0);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);
  assert(read_captured_terminal_byte() == 'A');
}

static void test_non_debug_uart_output_uses_stdout(void) {
  FILE *stdout_capture = tmpfile();
  assert(stdout_capture != NULL);
  int saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  debug_mode = false;
  send_uart_byte('B');
  fflush(stdout);
  rewind(stdout_capture);
  assert(fgetc(stdout_capture) == 'B');
  assert(fgetc(stdout_capture) == EOF);

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);
}

int main(void) {
  test_debug_uart_output_is_captured();
  test_non_debug_uart_output_uses_stdout();
  return 0;
}

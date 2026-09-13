#include "../include/parse/parse_args.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void send_uart_byte(uint8_t byte) {
  uart[0] = byte;
  uart[2] &= 0b11111110;
  update_uart();
}

static void send_uart_bytes(const uint8_t *bytes, size_t len) {
  for (size_t index = 0; index < len; index++) {
    send_uart_byte(bytes[index]);
  }
}

int main(void) {
  const uint8_t command[] = "\x1bliteral-output 17\x1b/";
  const uint8_t literal_output[] = "A\x1bwrite stdout\x1b/B";
  const uint8_t next_command[] = "\x1bwrite stdout\x1b/C";
  FILE *stdout_capture = tmpfile();
  uint8_t captured[18];
  int saved_stdout;

  assert(stdout_capture != NULL);
  saved_stdout = dup(STDOUT_FILENO);
  assert(saved_stdout >= 0);
  assert(dup2(fileno(stdout_capture), STDOUT_FILENO) >= 0);

  peripherals_dir = "/tmp";
  debug_mode = false;
  init_uart();
  max_waiting_instrs = 0;
  send_uart_bytes(command, sizeof(command) - 1);
  send_uart_bytes(literal_output, sizeof(literal_output) - 1);
  send_uart_bytes(next_command, sizeof(next_command) - 1);
  close_uart_output();
  free(uart);
  uart = NULL;

  fflush(stdout);
  rewind(stdout_capture);
  assert(fread(captured, 1, sizeof(captured), stdout_capture) ==
         sizeof(captured));
  assert(fgetc(stdout_capture) == EOF);
  assert(memcmp(captured, literal_output, sizeof(literal_output) - 1) == 0);
  assert(captured[17] == 'C');

  assert(dup2(saved_stdout, STDOUT_FILENO) >= 0);
  close(saved_stdout);
  fclose(stdout_capture);
  return 0;
}

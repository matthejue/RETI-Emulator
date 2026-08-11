#include "../include/special_opts.h"
#include "../include/uart.h"
#include "../include/parse/parse_args.h"
#include <assert.h>
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

int main() {
  char empty_filename[] = "/tmp/uart_load_empty_XXXXXX";
  char missing_filename[128];
  char command[256];
  int empty_file = mkstemp(empty_filename);
  assert(empty_file >= 0);
  close(empty_file);

  snprintf(missing_filename, sizeof(missing_filename),
           "/tmp/uart_load_missing_%ld.bin", (long)getpid());
  assert(access(missing_filename, F_OK) != 0);

  init_uart();
  max_waiting_instrs = 0;
  free(uart_input);
  uart_input = NULL;
  input_len = 0;
  input_idx = 0;

  snprintf(command, sizeof(command), "\x1bload %s\x1b/", missing_filename);
  send_uart_bytes((const uint8_t *)command, strlen(command));
  const uint8_t missing_word_count[] = {255, 255, 255, 255};
  assert(input_len == sizeof(missing_word_count));
  assert(memcmp(uart_input, missing_word_count,
                sizeof(missing_word_count)) == 0);

  snprintf(command, sizeof(command), "\x1bload %s\x1b/", empty_filename);
  send_uart_bytes((const uint8_t *)command, strlen(command));
  const uint8_t empty_word_count[] = {0, 0, 0, 0};
  assert(input_len == sizeof(missing_word_count) + sizeof(empty_word_count));
  assert(memcmp(uart_input + sizeof(missing_word_count), empty_word_count,
                sizeof(empty_word_count)) == 0);

  free(uart_input);
  close_uart_output();
  free(uart);
  remove(empty_filename);
  return 0;
}

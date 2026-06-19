#include "../../include/uart.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/interrupt_controller.h"
#include "../../include/input_output.h"
#include "../../include/special_opts.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t *uart_input = NULL;
uint16_t input_len = 0;
uint16_t input_idx = 0;

uint8_t receive_current_byte = '\0';

uint8_t sending_waiting_time = 0;
uint8_t receiving_waiting_time = 0;

typedef enum { UART_IDLE, UART_WAITING } Uart_State;

static Uart_State send_state = UART_IDLE;
static Uart_State receive_state = UART_IDLE;

char *all_send_data = NULL;
char *current_send_data = NULL;
uint16_t all_send_data_len = 0;
uint16_t current_send_data_len = 0;

uint8_t *uart;

#define UART_SEND_READY 0b00000001
#define UART_RECEIVE_READY 0b00000010
#define UART_INPUT_BOX_LEN 80

static bool is_visible_terminal_ascii(uint8_t byte) {
  return byte >= 32 && byte <= 126;
}

const char *format_uart_byte(uint8_t byte, char *buffer) {
  switch (byte) {
  case '\n':
    return "\\n";
  case '\t':
    return "\\t";
  case '\r':
    return "\\r";
  case '\0':
    return "\\0";
  case '\a':
    return "\\a";
  case '\b':
    return "\\b";
  case '\f':
    return "\\f";
  case '\v':
    return "\\v";
  case '\\':
    return "\\\\";
  }

  if (is_visible_terminal_ascii(byte)) {
    buffer[0] = byte;
    buffer[1] = '\0';
  } else {
    snprintf(buffer, 6, "\\d%03u", byte);
  }

  return buffer;
}

void init_uart() {
  uart = malloc(sizeof(uint8_t) * NUM_PERIPHERY_ADDRESSES);
  memset(uart, 0, sizeof(uint8_t) * NUM_PERIPHERY_ADDRESSES);
  uart[2] = UART_SEND_READY | UART_RECEIVE_READY;
  init_interrupt_controller();
}

static bool start_waiting(uint8_t *waiting_time) {
  if (max_waiting_instrs == 0) {
    *waiting_time = 0;
    return false;
  }

  *waiting_time = rand() % max_waiting_instrs + 1;
  return true;
}

static bool waiting_finished(uint8_t *waiting_time) {
  (*waiting_time)--;
  return *waiting_time == 0;
}

static bool is_digit(uint8_t ch) {
  return ch >= '0' && ch <= '9';
}

static void append_newline(uint8_t *input, uint16_t *len) {
  input[(*len)++] = '\n';
  input[*len] = '\0';
}

static void append_byte_to_buffer(char **buffer, uint16_t *len, uint8_t byte) {
  *buffer = realloc(*buffer, *len + 1);
  (*buffer)[(*len)++] = byte;
}

static void remember_sent_byte(uint8_t sent_byte) {
  if (!debug_mode) {
    return;
  }

  free(current_send_data);
  current_send_data = NULL;
  current_send_data_len = 0;
  append_byte_to_buffer(&current_send_data, &current_send_data_len, sent_byte);
  append_byte_to_buffer(&all_send_data, &all_send_data_len, sent_byte);
}

static bool uart_send_requested(void) {
  return !(read_array(uart, 2, true) & UART_SEND_READY);
}

static bool uart_receive_requested(void) {
  return !(read_array(uart, 2, true) & UART_RECEIVE_READY);
}

static void complete_send(void) {
  uint8_t sent_byte = uart[0];
  if (test_mode) {
    char buffer[6];
    adjust_print(true, "%s", "%s", format_uart_byte(sent_byte, buffer));
  } else {
    adjust_print(true, "%c", "%c", sent_byte);
  }
  remember_sent_byte(sent_byte);
  uart[2] = uart[2] | UART_SEND_READY;
  send_state = UART_IDLE;
}

static void update_uart_send(void) {
  switch (send_state) {
  case UART_IDLE:
    if (!uart_send_requested()) {
      return;
    }

    if (start_waiting(&sending_waiting_time)) {
      send_state = UART_WAITING;
    } else {
      complete_send();
    }
    break;
  case UART_WAITING:
    if (waiting_finished(&sending_waiting_time)) {
      complete_send();
    }
    break;
  }
}

static void set_uart_input_buffer(const uint8_t *input, uint16_t len) {
  free(uart_input);
  uart_input = malloc(len + 1);
  memcpy(uart_input, input, len);
  uart_input[len] = '\0';
  input_len = len;
  input_idx = 0;
}

uint16_t decode_uart_input_escapes(uint8_t *input, uint16_t len) {
  uint16_t read_idx = 0;
  uint16_t write_idx = 0;

  while (read_idx < len) {
    if (input[read_idx] == '\\' && read_idx + 1 < len) {
      switch (input[read_idx + 1]) {
      case 'n':
        input[write_idx++] = '\n';
        read_idx += 2;
        continue;
      case 't':
        input[write_idx++] = '\t';
        read_idx += 2;
        continue;
      case 'r':
        input[write_idx++] = '\r';
        read_idx += 2;
        continue;
      case '0':
        input[write_idx++] = '\0';
        read_idx += 2;
        continue;
      case 'a':
        input[write_idx++] = '\a';
        read_idx += 2;
        continue;
      case 'b':
        input[write_idx++] = '\b';
        read_idx += 2;
        continue;
      case 'f':
        input[write_idx++] = '\f';
        read_idx += 2;
        continue;
      case 'v':
        input[write_idx++] = '\v';
        read_idx += 2;
        continue;
      case '\\':
        input[write_idx++] = '\\';
        read_idx += 2;
        continue;
      }
    }

    if (input[read_idx] == '\\' && read_idx + 4 < len &&
        input[read_idx + 1] == 'd' && is_digit(input[read_idx + 2]) &&
        is_digit(input[read_idx + 3]) && is_digit(input[read_idx + 4])) {
      uint16_t value = (input[read_idx + 2] - '0') * 100 +
                       (input[read_idx + 3] - '0') * 10 +
                       (input[read_idx + 4] - '0');
      if (value <= UINT8_MAX) {
        input[write_idx++] = (uint8_t)value;
        read_idx += 5;
      } else {
        input[write_idx++] = input[read_idx++];
      }
      continue;
    }

    input[write_idx++] = input[read_idx++];
  }

  input[write_idx] = '\0';
  return write_idx;
}

static void ask_for_uart_input(void) {
  uint8_t input[UART_INPUT_BOX_LEN + 2];
  display_input_box((char *)input, "UART input (empty = newline):",
                    UART_INPUT_BOX_LEN);

  uint16_t len;
  if (input[0] == '\0') {
    input[0] = '\n';
    input[1] = '\0';
    len = 1;
  } else {
    len = decode_uart_input_escapes(input, strlen((char *)input));
    append_newline(input, &len);
  }

  set_uart_input_buffer(input, len);
}

static uint8_t next_receive_byte(void) {
  if (input_idx >= input_len) {
    ask_for_uart_input();
  }

  return uart_input[input_idx++];
}

static void complete_receive(void) {
  uart[1] = receive_current_byte;
  uart[2] = uart[2] | UART_RECEIVE_READY;
  receive_state = UART_IDLE;
}

static void update_uart_receive(void) {
  switch (receive_state) {
  case UART_IDLE:
    if (!uart_receive_requested()) {
      return;
    }

    receive_current_byte = next_receive_byte();
    if (start_waiting(&receiving_waiting_time)) {
      receive_state = UART_WAITING;
    } else {
      complete_receive();
    }
    break;
  case UART_WAITING:
    if (waiting_finished(&receiving_waiting_time)) {
      complete_receive();
    }
    break;
  }
}

void update_uart(void) {
  update_uart_receive();
  update_uart_send();
}

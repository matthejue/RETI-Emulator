#include "../../include/uart.h"
#include "../../include/interrupt.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/interrupt_controller.h"
#include "../../include/input_output.h"
#include "../../include/special_opts.h"
#include "../../include/terminal_view.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

uint8_t *uart_input = NULL;
size_t input_len = 0;
size_t input_idx = 0;

uint8_t receive_current_byte = '\0';

uint8_t sending_waiting_time = 0;
uint8_t receiving_waiting_time = 0;

typedef enum { UART_IDLE, UART_WAITING } Uart_State;

static Uart_State send_state = UART_IDLE;
static Uart_State receive_state = UART_IDLE;

char *all_send_data = NULL;
char *current_send_data = NULL;
size_t all_send_data_len = 0;
size_t current_send_data_len = 0;

uint8_t *uart;

#define UART_SEND_READY 0b00000001
#define UART_RECEIVE_READY 0b00000010
#define UART_INPUT_BOX_LEN 80
#define UART_LOAD_COMMAND_PREFIX "load "
#define UART_LOAD_COMMAND_MAX 4096

static char uart_load_command[UART_LOAD_COMMAND_MAX + 1];
static size_t uart_load_command_len = 0;
static bool uart_load_command_candidate = true;

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
  write_array(uart, SYSTEM_INFO_TIMER_INTERRUPT_INTERVAL,
              interrupt_timer_interval, true);
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

static void append_newline(uint8_t *input, size_t *len) {
  input[(*len)++] = '\n';
  input[*len] = '\0';
}

static void append_byte_to_buffer(char **buffer, size_t *len, uint8_t byte) {
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

static void append_uart_input_bytes(const uint8_t *input, size_t len) {
  if (len == 0) {
    return;
  }
  if (SIZE_MAX - input_len <= len) {
    fprintf(stderr, "Error: UART input buffer too large\n");
    exit(EXIT_FAILURE);
  }

  uint8_t *new_input = realloc(uart_input, input_len + len + 1);
  if (new_input == NULL) {
    fprintf(stderr, "Error: Couldn't allocate UART input buffer\n");
    exit(EXIT_FAILURE);
  }

  memcpy(new_input + input_len, input, len);
  input_len += len;
  new_input[input_len] = '\0';
  uart_input = new_input;
}

static void append_uart_input_word_count(size_t file_len) {
  size_t num_words = file_len / sizeof(uint32_t);
  uint32_t word_count = (uint32_t)num_words;
  uint8_t word_count_bytes[sizeof(uint32_t)] = {
      (uint8_t)(word_count >> 24),
      (uint8_t)(word_count >> 16),
      (uint8_t)(word_count >> 8),
      (uint8_t)word_count,
  };
  append_uart_input_bytes(word_count_bytes, sizeof(word_count_bytes));
}

static void load_file_into_uart_input(const char *path, size_t len) {
  if (len / sizeof(uint32_t) > UINT32_MAX) {
    fprintf(stderr, "Warning: UART input file %s contains too many words\n",
            path);
    return;
  }

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Warning: Couldn't load UART input file %s: %s\n", path,
            strerror(errno));
    return;
  }

  uint8_t *buffer = malloc(len == 0 ? 1 : len);
  if (buffer == NULL) {
    fclose(file);
    fprintf(stderr, "Error: Couldn't allocate UART input file buffer\n");
    exit(EXIT_FAILURE);
  }

  size_t bytes_read = fread(buffer, 1, len, file);
  if (bytes_read != len) {
    fprintf(stderr, "Warning: Couldn't read complete UART input file %s\n",
            path);
  }
  fclose(file);

  append_uart_input_word_count(bytes_read);
  append_uart_input_bytes(buffer, bytes_read);
  free(buffer);
}

static void process_uart_load_command(void) {
  const size_t prefix_len = strlen(UART_LOAD_COMMAND_PREFIX);
  if (strncmp(uart_load_command, UART_LOAD_COMMAND_PREFIX, prefix_len) != 0) {
    return;
  }

  char *path = uart_load_command + prefix_len;
  while (*path == ' ' || *path == '\t') {
    path++;
  }

  char *end = path + strlen(path);
  while (end > path && (end[-1] == ' ' || end[-1] == '\t')) {
    *--end = '\0';
  }
  if (*path == '\0') {
    return;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    return;
  }
  if (S_ISDIR(st.st_mode)) {
    fprintf(stderr,
            "Warning: UART load directory chooser is not implemented; send "
            "load <file> instead\n");
    return;
  }
  if (!S_ISREG(st.st_mode)) {
    return;
  }
  if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX) {
    fprintf(stderr, "Warning: UART input file %s is too large to load\n", path);
    return;
  }

  load_file_into_uart_input(path, (size_t)(uintmax_t)st.st_size);
}

void uart_handle_sent_byte_for_load_command(uint8_t byte) {
  if (byte == '\n' || byte == '\r') {
    if (uart_load_command_candidate) {
      uart_load_command[uart_load_command_len] = '\0';
      process_uart_load_command();
    }
    uart_load_command_len = 0;
    uart_load_command[0] = '\0';
    uart_load_command_candidate = true;
    return;
  }

  if (!uart_load_command_candidate) {
    return;
  }
  if (uart_load_command_len >= UART_LOAD_COMMAND_MAX) {
    uart_load_command_len = 0;
    uart_load_command[0] = '\0';
    uart_load_command_candidate = false;
    return;
  }

  uart_load_command[uart_load_command_len++] = byte;
  size_t prefix_len = strlen(UART_LOAD_COMMAND_PREFIX);
  if (uart_load_command_len <= prefix_len &&
      strncmp(uart_load_command, UART_LOAD_COMMAND_PREFIX,
              uart_load_command_len) != 0) {
    uart_load_command_len = 0;
    uart_load_command[0] = '\0';
    uart_load_command_candidate = false;
  }
}

static bool uart_send_requested(void) {
  return !(read_array(uart, 2, true) & UART_SEND_READY);
}

static bool uart_receive_requested(void) {
  return !(read_array(uart, 2, true) & UART_RECEIVE_READY);
}

static void complete_send(void) {
  uint8_t sent_byte = uart[0];
  if (debug_mode) {
    append_terminal_output(sent_byte);
  } else if (test_mode) {
    char buffer[6];
    adjust_print(true, "%s", "%s", format_uart_byte(sent_byte, buffer));
  } else {
    adjust_print(true, "%c", "%c", sent_byte);
  }
  remember_sent_byte(sent_byte);
  uart_handle_sent_byte_for_load_command(sent_byte);
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

static void set_uart_input_buffer(const uint8_t *input, size_t len) {
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

static void normalize_uart_input(uint8_t *input, size_t *len) {
  if (input[0] == '\0') {
    input[0] = '\n';
    input[1] = '\0';
    *len = 1;
  } else {
    *len = decode_uart_input_escapes(input, strlen((char *)input));
    append_newline(input, len);
  }
}

static void ask_for_uart_input(void) {
  uint8_t input[UART_INPUT_BOX_LEN + 2];
  size_t len;

  if (debug_mode) {
    display_input_box((char *)input, "UART input (empty = newline):",
                      UART_INPUT_BOX_LEN);
  } else {
    fflush(stdout);
    printf("\nUART input (empty = newline): ");
    fflush(stdout);

    if (fgets((char *)input, UART_INPUT_BOX_LEN + 1, stdin) == NULL) {
      input[0] = '\0';
    } else {
      len = strlen((char *)input);
      if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
      } else {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
      }
    }
  }

  normalize_uart_input(input, &len);

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

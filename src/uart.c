#include "../include/uart.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include "../include/special_opts.h"
#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t remaining_bytes = 0;
uint8_t num_bytes = 0;
uint16_t send_idx = 0;
uint8_t *send_data;

int8_t *uart_input = NULL;
uint8_t input_len = 0;
uint8_t input_idx = 0;

int8_t received_num = '\0';

uint8_t sending_waiting_time = 0;
uint8_t receiving_waiting_time = 0;

bool sending_finished = false;
bool receiving_finished = false;

bool init_finished = false;

DataType datatype;

void uart_send() {
  if (!(read_array(uart, 2, true) & 0b00000001) && !sending_finished) {
    if (!init_finished) {
      datatype = uart[0];
      switch (datatype) {
      case STRING:
        remaining_bytes = 0;
        send_data = NULL;
        break;
      case INTEGER:
        datatype = INTEGER;
        num_bytes = remaining_bytes = 4;
        send_data = malloc(remaining_bytes);
        memset(send_data, 0, remaining_bytes);
        break;
      default:
        fprintf(stderr, "Error: Invalid datatype\n");
        exit(EXIT_FAILURE);
      }
      init_finished = true;
    } else if (datatype == INTEGER) {
      send_data[num_bytes - remaining_bytes] = uart[0];
      remaining_bytes--;
      if (remaining_bytes == 0) {
        adjust_print(true, "%d\n", "%d ", swap_endian_32(*((uint32_t *)send_data)));

        init_finished = false;
      }
    } else if (datatype == STRING) {
      send_data = realloc(send_data, 1);
      uint8_t data = uart[0];
      send_data[send_idx] = data;
      send_idx++;
      if (data == 0) {
        adjust_print(true, "%s\n", "%s ", send_data);

        send_idx = 0;
        init_finished = false;
      }
    } else {
      fprintf(stderr, "Error: Invalid datatype\n");
      exit(EXIT_FAILURE);
    }

    if (max_waiting_instrs == 0) {
      goto sending_finished;
    } else {
      sending_waiting_time = rand() % max_waiting_instrs + 1;
    }
    sending_finished = true;
  } else if (sending_finished) {
  sending_finished:
    sending_waiting_time--;
    if (sending_waiting_time == 0) {
      uart[2] = uart[2] | 0b00000001;
      sending_finished = false;
    }
  }
}

void ask_for_user_input() {
  int8_t input[4];

  while (true) {
    printf("Please enter a number or a character: ");
    if (fgets((char *)input, sizeof(input), stdin) == NULL) {
      fprintf(stderr, "Error: Couldn't read input\n");
    }
    uint8_t idx_of_newline = strcspn((char *)input, "\n");
    if (idx_of_newline > 3) {
      fprintf(stderr, "Error: Input with more than 3 characters not possible, the "
             "resuliting number would definitely by out of range\n");
      exit(EXIT_FAILURE);
    }
    input[idx_of_newline] = '\0';

    if (isalpha(input[0])) {
      if (strlen((char *)input) > 1) {
        fprintf(stderr, "Error: Only one character allowed\n");
      } else {
        received_num = input[0];
        break;
      }
    } else if (isdigit(input[0]) || input[0] == '-') {
      char *endptr;
      received_num = strtol((char *)input, &endptr, 10);
      if (*endptr != '\0') {
        const char *str = "Error: Further characters after number: ";
        const char *str2 = proper_str_cat(str, endptr);
        const char *str3 = proper_str_cat(str2, ".\n");
        fprintf(stderr, "%s", str3);
      } else if (received_num < INT8_MIN && received_num > INT8_MAX) {
        fprintf(stderr, "Error: Number out of range, must be between -128 and 127\n");
      } else {
        break;
      }
    } else {
      fprintf(stderr, "Error: Invalid input\n");
    }
  }
}

void uart_receive() {
  if (!(read_array(uart, 2, true) & 0b00000010) && !receiving_finished) {
    if (read_metadata && input_idx < input_len) {
      received_num = uart_input[input_idx];
      input_idx++;
    } else {
      ask_for_user_input();
    }

    if (max_waiting_instrs == 0) {
      goto receiving_finished;
    } else {
      receiving_waiting_time = rand() % max_waiting_instrs + 1;
    }
    receiving_finished = true;
  } else if (receiving_finished) {
  receiving_finished:
    receiving_waiting_time--;
    if (receiving_waiting_time == 0) {
      uart[1] = received_num; // & 0xFF; not necessary
      uart[2] = uart[2] | 0b00000010;
      receiving_finished = false;
    }
  }
}

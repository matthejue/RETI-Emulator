#include "../include/uart.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/special_opts.h"
#include "../include/utils.h"
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

uint32_t *uart_input = NULL;
uint8_t input_len = 0;
uint8_t input_idx = 0;

uint32_t received_num;
uint8_t received_num_part = '\0';
uint8_t received_num_idx = -1;

uint8_t sending_waiting_time = 0;
uint8_t receiving_waiting_time = 0;

bool sending_finished = false;
bool receiving_finished = false;

bool init_finished = false;

DataType datatype;

char *all_send_data = NULL;
char *current_send_data = NULL;

void uart_send() {
  if (!(read_array(uart, 2, true) & 0b00000001) && !sending_finished) {
    if (!init_finished) {
      datatype = uart[0];
      switch (datatype) {
      case STRING:
        send_idx = 0;
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
    } else if (datatype == INTEGER) {
      send_data[num_bytes - remaining_bytes] = uart[0];
    } else if (datatype == STRING) {
      send_data = realloc(send_data, 1);
      send_data[send_idx] = uart[0];
      // TODO: ist send_idx nicht unnötig, weil es eh immer die letzte Stelle
      // ist?
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
    sending_waiting_time--;
    if (sending_waiting_time == 0) {
    sending_finished:
      if (datatype == STRING) {
        if (!init_finished) {
          if (debug_mode) {
            current_send_data = malloc(3);
            current_send_data[0] = '0';
            current_send_data[1] = ' ';
            current_send_data[2] = '\0';
          }
          init_finished = true;
        } else {
          if (debug_mode) {
            current_send_data =
                realloc(current_send_data, strlen(current_send_data) + 2);
            sprintf(current_send_data + strlen(current_send_data), "%c",
                    send_data[send_idx]);
          }
          send_idx++;
          if (uart[0] == 0) {
            adjust_print(true, "%s\n", "%s ", send_data);
            if (debug_mode) {
              uint8_t len_new_data = strlen((char *)send_data);
              if (all_send_data) {
                all_send_data = realloc(all_send_data, strlen(all_send_data) +
                                                           len_new_data + 2);
              } else {
                all_send_data = realloc(all_send_data, len_new_data + 2);
                all_send_data[0] = '\0';
              }
              sprintf(all_send_data + strlen(all_send_data), "%s ", send_data);
            }

            init_finished = false;
          }
        }
      } else if (datatype == INTEGER) {
        if (!init_finished) {
          if (debug_mode) {
            current_send_data = malloc(3);
            current_send_data[0] = '4';
            current_send_data[1] = ' ';
            current_send_data[2] = '\0';
          }
          init_finished = true;
        } else {
          if (debug_mode) {
            uint8_t num = send_data[num_bytes - remaining_bytes];
            uint8_t len_new_data = num_digits_for_num(num);
            current_send_data =
                realloc(current_send_data,
                        strlen(current_send_data) + len_new_data + 2);
            sprintf(current_send_data + strlen(current_send_data), "%d ", num);
          }
          remaining_bytes--;
          if (remaining_bytes == 0) {
            uint32_t num = swap_endian_32(*((uint32_t *)send_data));
            adjust_print(true, "%d\n", "%d ", num);
            if (debug_mode) {
              uint8_t len_new_data = num_digits_for_num(num);
              if (all_send_data) {
                all_send_data = realloc(all_send_data, strlen(all_send_data) +
                                                           len_new_data + 2);
              } else {
                all_send_data = realloc(all_send_data, len_new_data + 2);
                all_send_data[0] = '\0';
              }
              sprintf(all_send_data + strlen(all_send_data), "%d ", num);
            }

            init_finished = false;
          }
        }
      } else {
        fprintf(stderr, "Error: Invalid datatype\n");
        exit(EXIT_FAILURE);
      }
      // TODO: für send data vielleict einbauen, dass es erst hier dann
      // angezeigt wird, wenn die waiting time abgelaufen ist
      uart[2] = uart[2] | 0b00000001;
      sending_finished = false;
    }
  }
}

uint32_t ask_for_user_input() {
  uint8_t input[13];

  while (true) {
    printf("Number between -2147483648 and 2147483647 or a character: ");
    if (fgets((char *)input, sizeof(input), stdin) == NULL) {
      fprintf(stderr, "Error: Couldn't read input\n");
    } else {
      // Find the position of the newline character
      uint8_t idx_of_newline = strcspn((char *)input, "\n");
      // If the newline character is not found, it means the input was too
      // long
      if (input[idx_of_newline] != '\n') {
        // Clear the input buffer
        uint32_t c;
        while ((c = getchar()) != '\n' && c != EOF)
          ;
        fprintf(stderr, "Error: Input too long\n");
        continue;
      } else {
        // Replace the newline character with a null terminator
        input[idx_of_newline] = '\0';
      }
    }

    if (isalpha(input[0])) {
      if (strlen((char *)input) > 1) {
        fprintf(stderr, "Error: Only one character allowed\n");
      } else {
        return *(uint32_t *)input;
      }
    } else if (isdigit(input[0]) || input[0] == '-') {
      char *endptr;
      uint64_t tmp_num = strtol((char *)input, &endptr, 10);
      if (*endptr != '\0') {
        const char *str = "Error: Further characters after number: ";
        const char *str2 = proper_str_cat(str, endptr);
        const char *str3 = proper_str_cat(str2, ".\n");
        fprintf(stderr, "%s", str3);
      } else if ((int64_t)tmp_num < INT32_MIN || (int64_t)tmp_num > INT32_MAX) {
        fprintf(stderr, "Error: Number out of range, must be between "
                        "-2147483648 and 2147483647\n");
      } else {
        return tmp_num;
      }
    } else {
      fprintf(stderr, "Error: Invalid input\n");
    }
  }
}

void uart_receive() {
  if (!(read_array(uart, 2, true) & 0b00000010) && !receiving_finished) {
    if ((int8_t)received_num_idx == -1) {
      if (read_metadata && input_idx < input_len) {
        received_num = uart_input[input_idx];
      } else {
        received_num = ask_for_user_input();
      }
      received_num_idx = 3;
    }
    received_num_part = (received_num & (0xFF << (received_num_idx * 8))) >>
                        (received_num_idx * 8);
    received_num_idx--;

    if (read_metadata && input_idx < input_len &&
        (int8_t)received_num_idx == -1) {
      input_idx++;
    }

    if (max_waiting_instrs == 0) {
      goto receiving_finished;
    } else {
      receiving_waiting_time = rand() % max_waiting_instrs + 1;
    }
    receiving_finished = true;
  } else if (receiving_finished) {
    receiving_waiting_time--;
    if (receiving_waiting_time == 0) {
    receiving_finished:
      uart[1] = received_num_part; // & 0xFF; not necessary
      uart[2] = uart[2] | 0b00000010;
      receiving_finished = false;
    }
  }
}

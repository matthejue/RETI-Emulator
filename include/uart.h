#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef UART_H
#define UART_H

extern uint8_t *uart_input;
extern size_t input_len;
extern size_t input_idx;

extern uint8_t receive_current_byte;

extern uint8_t sending_waiting_time;
extern uint8_t receiving_waiting_time;

extern char *all_send_data;
extern char *current_send_data;
extern size_t all_send_data_len;
extern size_t current_send_data_len;

extern uint8_t *uart;

void update_uart(void);
void init_uart() ;
uint16_t decode_uart_input_escapes(uint8_t *input, uint16_t len);
const char *format_uart_byte(uint8_t byte, char *buffer);
void uart_handle_sent_byte_for_load_command(uint8_t byte);

#endif // UART_H

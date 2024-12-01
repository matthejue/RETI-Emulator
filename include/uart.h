#include <stdbool.h>
#include <stdint.h>

#define UART_H
#ifdef UART_H

extern uint8_t remaining_bytes;
extern uint8_t num_bytes;
extern uint16_t send_idx;
extern uint8_t *send_data;

extern uint32_t *uart_input;
extern uint8_t input_len;
extern uint8_t input_idx;

extern uint32_t received_num;
extern uint8_t received_num_part;
extern uint8_t received_num_idx;

extern uint8_t sending_waiting_time;
extern uint8_t receiving_waiting_time;

typedef enum { STRING, INTEGER = 4 } DataType;

#define MAX_NUM_DIGITS_INTEGER 11 // log(2**32, 10) + possible minus sign

extern DataType datatype;

extern char *all_send_data;
extern char *current_send_data;

void uart_send();
void uart_receive();
bool display_input_message(char *input, const char *message,
                           uint8_t max_num_digits);
void display_error_notification(const char *message);
bool ask_for_user_input(char *input, char *message, uint8_t max_num_digits);
void display_input_error(const char *message);
uint32_t get_user_input();

#endif // UART_H

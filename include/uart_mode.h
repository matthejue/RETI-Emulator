#ifndef UART_MODE_H
#define UART_MODE_H

#include <stdbool.h>
#include <stdint.h>

bool activate_uart_mode(void);
bool debug_key_to_uart_byte(int key, uint8_t *byte);
void update_uart_mode(void);
void close_uart_mode(void);

#endif // UART_MODE_H

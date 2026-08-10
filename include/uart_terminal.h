#ifndef UART_TERMINAL_H
#define UART_TERMINAL_H

#include <stdbool.h>

bool activate_uart_terminal(bool raw_input);
bool uart_terminal_is_active(void);
void update_uart_terminal(void);
void wait_for_uart_terminal_exit(void);
void close_uart_terminal(void);

#endif // UART_TERMINAL_H

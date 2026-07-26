#ifndef TERMINAL_VIEW_H
#define TERMINAL_VIEW_H

#include <stdbool.h>
#include <stdint.h>

bool init_terminal_output(void);
void append_terminal_output(uint8_t byte);
bool read_terminal_input(uint8_t *byte);
void discard_terminal_input(void);
bool start_terminal_viewer(void);
void stop_terminal_viewer(void);
void close_terminal_output(void);

#endif // TERMINAL_VIEW_H

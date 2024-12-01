#ifndef TUI_H
#define TUI_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  char *title;
  uint8_t x, y;
  uint8_t width, height;
  uint8_t line, col;
} Box;

extern uint16_t term_width, term_height;

extern Box regs_box;
extern Box eprom_box;
extern Box uart_box;
extern Box sram_c_box;
extern Box sram_d_box;
extern Box sram_s_box;

extern Box *boxes[];
extern uint8_t box_length;

void write_text_into_box(Box *box, const char *text);
void draw_box(Box *box);

void init_tui();
void fin_tui();

#define HEIGHT_REGS_BOX 10
#define HEIGHT_UART_BOX 11

bool display_input_box(char *input, const char *message,
                       uint8_t max_num_digits);
void reset_box_line(Box *box);
void update_term_and_box_sizes();

#endif // TUI_H

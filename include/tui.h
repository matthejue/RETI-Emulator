#ifndef TUI_H
#define TUI_H

#include <stdint.h>

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

void display_input_box(char *input, const char *message);
void reset_box_line(Box *box);
void display_input_box(char *input, const char *message) ;

#endif // TUI_H

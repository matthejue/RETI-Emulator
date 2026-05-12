#ifndef TUI_H
#define TUI_H

#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  char *title;
  uint8_t x, y;
  uint8_t width, height;
  uint8_t line, col;
  WINDOW *win;
} Box;

extern uint16_t term_width, term_height;

extern Box regs_box;
extern Box eprom_box;
extern Box uart_box;
extern Box sram_c_box;
extern Box sram_d_box;
extern Box sram_s_box;
extern Box info_box;

extern Box *boxes[];
extern const uint8_t NUM_BOXES;

void write_text_into_box(Box *box, const char *text);
void write_text_into_box_with_attr(Box *box, const char *text, int attr);
void draw_boxes();
void set_tui_active_box(Box *box);

#define COMMENT_COLOR_PAIR 1
#define DEBUG_VARIABLE_COLOR_PAIR 2
#define ACTIVE_TITLE_COLOR_PAIR 3

void init_tui();
void fin_tui();

#define HEIGHT_REGS_BOX 10
#define HEIGHT_UART_BOX 11

void reset_box_line(Box *box);
void make_unneccessary_spaces_visible(Box *box);
void update_term_and_box_sizes();
void cycle_info_box_page(void);
void set_tui_halted_mode(bool halted);
void set_tui_snapshot_available(bool available);

#endif // TUI_H

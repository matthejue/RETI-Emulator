#include "../include/tui.h"
#include "../include/parse_args.h"
#include <ncurses.h>
#include <stdint.h>
#include <string.h>

Box regs_box = {"Register", 0, 0, 0, 0, 1, 1};
Box eprom_box = {"EPROM (e): PC (0)", 0, 0, 0, 0, 1, 1};
Box uart_box = {"UART", 0, 0, 0, 0, 1, 1};
Box sram_c_box = {"SRAM Codesegment (sc): PC (0)", 0, 0, 0, 0, 1, 1};
Box sram_d_box = {"SRAM Datasegment (sd): DS (0)", 0, 0, 0, 0, 1, 1};
Box sram_s_box = {"SRAM Stack (ss): SP (0)", 0, 0, 0, 0, 1, 1};

uint16_t term_width, term_height;

Box *boxes[] = {&regs_box,   &eprom_box,  &uart_box,
                &sram_c_box, &sram_d_box, &sram_s_box};
uint8_t box_length = sizeof(boxes) / sizeof(boxes[0]);

void init_tui() {
  initscr();
  cbreak();
  noecho();
  curs_set(0); // Hide cursor

  getmaxyx(stdscr, term_height, term_width);

  int first_box_width = term_width / 4;
  int remaining_width = term_width - first_box_width;
  int other_box_width = remaining_width / 3;
  int box_height = term_height-1;

  int first_box_height = 12;
  int third_box_height = 11;
  int second_box_height = box_height - first_box_height - third_box_height;

  regs_box.x = 0;
  regs_box.y = 0;
  regs_box.width = first_box_width;
  regs_box.height = first_box_height;

  eprom_box.x = 0;
  eprom_box.y = first_box_height;
  eprom_box.width = first_box_width;
  eprom_box.height = second_box_height;

  uart_box.x = 0;
  uart_box.y = first_box_height + second_box_height;
  uart_box.width = first_box_width;
  uart_box.height = third_box_height;

  sram_c_box.x = first_box_width;
  sram_c_box.y = 0;
  sram_c_box.width = other_box_width;
  sram_c_box.height = box_height;

  sram_d_box.x = first_box_width + other_box_width;
  sram_d_box.y = 0;
  sram_d_box.width = other_box_width;
  sram_d_box.height = box_height;

  sram_s_box.x = first_box_width + 2 * other_box_width;
  sram_s_box.y = 0;
  sram_s_box.width = other_box_width;
  sram_s_box.height = box_height;
}

void fin_tui() {
  endwin(); // End ncurses mode
}

// Function to draw a box at specified position and size
void draw_box(Box *box) {
  int title_len = strlen(box->title);
  int title_start = box->x + (box->width - 1 - title_len - 4) /
                                2; // Calculate start position for title

  mvhline(box->y, box->x, 0, box->width - 1); // Top border
  mvprintw(box->y, title_start, " %s ",
           box->title); // Print title centered with spaces
  mvhline(box->y + box->height - 1, box->x, 0, box->width - 1);     // Bottom border
  mvvline(box->y + 1, box->x, 0, box->height - 1);                 // Left border
  mvvline(box->y + 1, box->x + box->width - 1, 0, box->height - 1); // Right border

  mvaddch(box->y, box->x, ACS_ULCORNER);                  // Top-left corner
  mvaddch(box->y, box->x + box->width - 1, ACS_URCORNER);  // Top-right corner
  mvaddch(box->y + box->height - 1, box->x, ACS_LLCORNER); // Bottom-left corner
  mvaddch(box->y + box->height - 1, box->x + box->width - 1,
          ACS_LRCORNER); // Bottom-right corner
}

void write_text_into_box(Box *box, const char *text) {
  uint8_t text_len = strlen(text);
  for (uint8_t i = 0; i < text_len; i++) {
    if (box->line >= (box->height - 1)) {
      break; // Stop if we exceed the box height
    }
    if (text[i] == '\n' || box->col >= (box->width - 1)) {
      box->line++;
      box->col = 1;
      if (text[i] == '\n') {
        continue;
      }
    }
    if (box->line <
        (box->height - 1)) { // Ensure we don't write on the bottom border
      mvaddch(box->y + box->line, box->x + box->col, text[i]);
      box->col++;
    }
  }
}

void reset_box_line(Box *box) { box->line = 1; }

void display_input_box(char *input, const char *message) {
  int max_length = 20;
  int box_width = 30;
  int box_height = 5;
  int startx = (term_width - box_width) / 2;
  int starty = (term_height - box_height) / 2;

  initscr();
  noecho();
  cbreak();
  keypad(stdscr, TRUE);

  WINDOW *input_box = newwin(box_height, box_width, starty, startx);
  box(input_box, 0, 0);
  mvwprintw(input_box, 0, (box_width - strlen(message)) / 2, "%s", message);
  wrefresh(input_box);

  echo();
  mvwgetnstr(input_box, 2, 1, input, max_length + 1);
  noecho();

  if (strlen(input) > max_length) {
    WINDOW *error_box = newwin(box_height, box_width, starty, startx);
    box(error_box, 0, 0);
    mvwprintw(error_box, 2, 1, "Input too long!");
    wrefresh(error_box);
    wgetch(error_box);
    delwin(error_box);
  }

  delwin(input_box);
  endwin();
}

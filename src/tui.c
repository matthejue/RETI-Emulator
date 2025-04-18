#include "../include/tui.h"
#include "../include/parse_args.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

Box regs_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box eprom_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box uart_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_c_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_d_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_s_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box info_box = {"(n)ext instruction, (c)ontinue to breakpoint, (r)estart, "
                "(s)tep into isr, (f)inalize isr, (t)rigger isr, "
                "(a)ssign watchobject reg or addr, (q)uit",
                0,
                0,
                0,
                0,
                1,
                1,
                NULL};

uint16_t term_width, term_height;

Box *boxes[] = {&regs_box,   &eprom_box,  &uart_box, &sram_c_box,
                &sram_d_box, &sram_s_box, &info_box};
const uint8_t NUM_BOXES = sizeof(boxes) / sizeof(boxes[0]);

void init_tui() {
  initscr();
  cbreak();
  noecho();
  curs_set(0); // Hide cursor

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    boxes[i]->win = newwin(1, 1, 0, 0);
  }
}

void update_term_and_box_sizes() {
  refresh(); // has to be because term_height and term_width can only be
             // determined after refresh
  getmaxyx(stdscr, term_height, term_width);

  uint16_t first_box_width = term_width / 4;
  uint16_t remaining_width = term_width - first_box_width;
  uint16_t other_box_width = remaining_width / 3;
  uint16_t box_height = term_height - 1;

  uint16_t first_box_height = HEIGHT_REGS_BOX;
  uint16_t third_box_height = HEIGHT_UART_BOX;
  uint16_t second_box_height = box_height - first_box_height - third_box_height;

  // regs_box.x = 0;
  // regs_box.y = 0;
  regs_box.width = first_box_width;
  regs_box.height = first_box_height;

  // eprom_box.x = 0;
  eprom_box.y = first_box_height;
  eprom_box.width = first_box_width;
  eprom_box.height = second_box_height;

  // uart_box.x = 0;
  uart_box.y = first_box_height + second_box_height;
  uart_box.width = first_box_width;
  uart_box.height = third_box_height;

  sram_c_box.x = first_box_width;
  // sram_c_box.y = 0;
  sram_c_box.width = other_box_width;
  sram_c_box.height = box_height;

  sram_d_box.x = first_box_width + other_box_width;
  // sram_d_box.y = 0;
  sram_d_box.width = other_box_width;
  sram_d_box.height = box_height;

  sram_s_box.x = first_box_width + 2 * other_box_width;
  // sram_s_box.y = 0;
  sram_s_box.width = other_box_width;
  sram_s_box.height = box_height;

  // info_box.x = 0;
  info_box.y = term_height - 1;
  info_box.width = term_width - 1;
  info_box.height = 1;

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    wresize(boxes[i]->win, boxes[i]->height,
            boxes[i]->width); // Resize the window
    mvwin(boxes[i]->win, boxes[i]->y,
          boxes[i]->x); // Move the window to a new position
  }
}

void fin_tui() {
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    delwin(boxes[i]->win);
  }

  endwin(); // End ncurses mode
}

void draw_boxes() {
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    const uint8_t TITLE_LEN = strlen(boxes[i]->title);
    uint16_t rel_pos =
        boxes[i]->width >= TITLE_LEN + 2
            ? (boxes[i]->width - TITLE_LEN - 2 /* 2 spaces */) / 2
            : 0;
    if (i < NUM_BOXES - 1) {
      box(boxes[i]->win, 0, 0);
    }
    mvwprintw(boxes[i]->win, 0, rel_pos == 0 ? 1 : rel_pos, " %.*s ",
              (uint32_t)min(boxes[i]->width - 4 /* 2 spaces + 2 corner */,
                            TITLE_LEN + 2),
              boxes[i]->title);
    wrefresh(boxes[i]->win);
  }
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
      mvwaddch(box->win, box->line, box->col, text[i]);
      box->col++;
    }
  }
}

void reset_box_line(Box *box) { box->line = 1; }

void make_unneccessary_spaces_visible(Box *box) {
  for (int i = 1; i < box->height - 1; i++) {
    for (int j = 1; j < box->width - 1; j++) {
      mvwaddch(box->win, i, j, '_');
    }
  }
}

#include "../include/tui.h"
#include "../include/assemble.h"
#include "../include/interrupt.h"
#include "../include/parse/parse_args.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

Box regs_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box eprom_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box uart_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_c_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_d_box = {"", 0, 0, 0, 0, 1, 1, NULL};
Box sram_s_box = {"", 0, 0, 0, 0, 1, 1, NULL};
static const char *info_box_pages[] = {
    "(n)ext instruction, (c)ontinue to breakpoint, (r)estart, "
    "(s)tep into isr, (f)inalize isr, "
    "(tab/S-tab) to switch, (q)uit, (o)ther actions",
};
static const char *halted_info_box_pages[] = {
    "Program halted, (r)estart, "
    "(tab/S-tab) to switch, (q)uit, (o)ther actions"};
static const uint8_t NUM_INFO_BOX_PAGES = 3;
static const uint8_t NUM_HALTED_INFO_BOX_PAGES = 3;
static uint8_t current_info_box_page = 0;
static char info_box_second_page[256];
static char info_box_third_page[256];
static bool tui_halted_mode = false;
static bool tui_snapshot_available = false;
static bool tui_uart_mode = false;

Box info_box = {"", 0, 0, 0, 0, 1, 1, NULL};
// Box paging_box = {"", 0, 0, 0, 0, 1, 1, NULL};

uint16_t term_width, term_height;

Box *boxes[] = {&regs_box,   &eprom_box,  &uart_box, &sram_c_box,
                &sram_d_box, &sram_s_box, &info_box};
const uint8_t NUM_BOXES = sizeof(boxes) / sizeof(boxes[0]);
static Box *active_box = &eprom_box;

static void update_info_box_text(void) {
  if (tui_uart_mode) {
    info_box.title =
        "Currently in (U)ART mode; press Escape to exit (U)ART mode";
    return;
  }

  if (current_info_box_page == 1) {
    if (tui_halted_mode) {
      snprintf(info_box_second_page, sizeof(info_box_second_page),
               "(j/k) to scroll, "
               "(J/K) to in/decrease watchobject, (C)enter, "
               "(a)ssign watchobject, "
               "(A)ssign value, (o)ther actions");
    } else {
      uint8_t custom_action_isr = get_custom_interrupt_action_isr();
      if (custom_action_isr == INVALID_ISR_NUM) {
        snprintf(
            info_box_second_page, sizeof(info_box_second_page),
            "(j/k) to scroll, "
            "(J/K) to in/decrease watchobject, (C)enter, "
            "(a)ssign watchobject, "
            "(A)ssign value, "
            "(T)rigger isr none, (e)xchange isr, (o)ther actions");
      } else {
        snprintf(
            info_box_second_page, sizeof(info_box_second_page),
            "(j/k) to scroll, "
            "(J/K) to in/decrease watchobject, (C)enter, "
            "(a)ssign watchobject, "
            "(A)ssign value, "
            "(T)rigger isr %u, (e)xchange isr, (o)ther actions",
            custom_action_isr);
      }
    }
    info_box.title = info_box_second_page;
    return;
  }

  if (current_info_box_page == 2) {
    snprintf(info_box_third_page, sizeof(info_box_third_page),
             tui_snapshot_available
                 ? "(S)napshot, (R)estore, (d)ebug source, "
                   "(V)iew terminal, (U)ART mode, "
                   "(t)ranscode, (o)ther actions"
                 : "(S)napshot, (d)ebug source, "
                   "(V)iew terminal, (U)ART mode, "
                   "(t)ranscode, (o)ther actions");
    info_box.title = info_box_third_page;
    return;
  }

  if (tui_halted_mode) {
    info_box.title = (char *)halted_info_box_pages[current_info_box_page];
    return;
  }

  info_box.title = (char *)info_box_pages[current_info_box_page];
}

void set_tui_snapshot_available(bool available) {
  tui_snapshot_available = available;
  update_info_box_text();
}

void set_tui_uart_mode(bool active) {
  tui_uart_mode = active;
  update_info_box_text();
}

void init_tui() {
  initscr();
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  curs_set(0); // Hide cursor
  if (has_colors()) {
    start_color();
    use_default_colors();
    init_pair(COMMENT_COLOR_PAIR, COLOR_WHITE, COLOR_BLACK);
    init_pair(DEBUG_VARIABLE_COLOR_PAIR, COLOR_WHITE, -1);
    init_pair(ACTIVE_TITLE_COLOR_PAIR, COLOR_BLACK, COLOR_WHITE);
    init_pair(WATCHOBJECT_COLOR_PAIR, COLOR_BLACK, COLOR_WHITE);
  }

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    boxes[i]->win = newwin(1, 1, 0, 0);
  }

  update_info_box_text();
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
  uint16_t remaining_first_column_height = box_height - first_box_height;
  uint16_t second_box_height = remaining_first_column_height / 2;
  uint16_t third_box_height =
      remaining_first_column_height - second_box_height;

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

void cycle_info_box_page(void) {
  current_info_box_page =
      (current_info_box_page + 1) %
      (tui_halted_mode ? NUM_HALTED_INFO_BOX_PAGES : NUM_INFO_BOX_PAGES);
  update_info_box_text();
}

void set_tui_halted_mode(bool halted) {
  tui_halted_mode = halted;
  current_info_box_page = 0;
  update_info_box_text();
}

void set_tui_active_box(Box *box) { active_box = box; }

void fin_tui() {
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    delwin(boxes[i]->win);
  }

  endwin(); // End ncurses mode
}

void draw_boxes() {
  update_info_box_text();

  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    const uint8_t TITLE_LEN = strlen(boxes[i]->title);
    uint16_t rel_pos =
        boxes[i]->width >= TITLE_LEN + 2
            ? (boxes[i]->width - TITLE_LEN - 2 /* 2 spaces */) / 2
            : 0;
    if (i < NUM_BOXES - 1) {
      box(boxes[i]->win, 0, 0);
    }
    if (boxes[i] == active_box) {
      wattron(boxes[i]->win, COLOR_PAIR(ACTIVE_TITLE_COLOR_PAIR));
    }
    mvwprintw(boxes[i]->win, 0, rel_pos == 0 ? 1 : rel_pos, " %.*s ",
              (uint32_t)min(boxes[i]->width - 4 /* 2 spaces + 2 corner */,
                            TITLE_LEN + 2),
              boxes[i]->title);
    if (boxes[i] == active_box) {
      wattroff(boxes[i]->win, COLOR_PAIR(ACTIVE_TITLE_COLOR_PAIR));
    }
    wrefresh(boxes[i]->win);
  }
}

void write_text_into_box(Box *box, const char *text) {
  write_text_into_box_with_attr(box, text, A_NORMAL);
}

void write_text_into_box_with_attr(Box *box, const char *text, int attr) {
  size_t text_len = strlen(text);
  for (size_t i = 0; i < text_len; i++) {
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
      wattron(box->win, attr);
      mvwaddch(box->win, box->line, box->col, text[i]);
      wattroff(box->win, attr);
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

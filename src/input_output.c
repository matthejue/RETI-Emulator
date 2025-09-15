#include "../include/input_output.h"
#include "../include/debug.h"
#include "../include/parse_args.h"
#include "../include/tui.h"
#include "../include/utils.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

bool display_notification_box_with_action(const char *title,
                                          const char *message, const char key,
                                          void (*action)(void),
                                          void (*action2)(void)) {
  const uint8_t LEN_ERROR = strlen(title);
  const uint8_t LEN_PRESS_ENTER = strlen("Press Enter to skip");
  const uint8_t LEN_MESSAGE = strlen(message);
  uint8_t box_width = max(LEN_MESSAGE + 4, LEN_PRESS_ENTER + 4);
  uint8_t box_height = 4;
  uint16_t startx = (term_width - box_width) / 2;
  uint16_t starty = (term_height - box_height) / 2;

  WINDOW *notification_box = newwin(box_height, box_width, starty, startx);
  box(notification_box, 0, 0);
  mvwprintw(notification_box, 0, (box_width - LEN_ERROR - 2) / 2, " %s ",
            title);
  mvwprintw(notification_box, 1, (box_width - LEN_MESSAGE) / 2, "%s", message);
  mvwprintw(notification_box, 2, (box_width - LEN_PRESS_ENTER) / 2,
            "Press Enter to skip");
  wrefresh(notification_box);

  bool should_cont = true;
  int ch;
  if (action == NULL && action2 == NULL) {
    while ((ch = wgetch(notification_box)) != '\n' &&
           ch != '\r') { // Wait for Enter key (newline or carriage return)
    }
  } else {
    while (true) {
      ch = wgetch(notification_box);
      if (ch == '\n' || ch == '\r') {
        if (action != NULL) {
          action();
        }
        break;
      } else if (ch == key) {
        if (action2 != NULL) {
          action2();
        }
        should_cont = false;
        break;
      }
    }
  }

  delwin(notification_box);
  draw_boxes();

  return should_cont;
}

void display_notification_box(const char *title, const char *message) {
  display_notification_box_with_action(title, message, '\0', NULL, NULL);
}

void display_input_box(char *input, const char *message,
                       uint8_t max_num_digits) {
  const uint8_t LEN_MESSAGE = strlen(message);
  uint8_t box_width = LEN_MESSAGE + 4; // 2 spaces, 2 corncer chrs
  uint8_t box_height = 3;
  uint16_t startx = (term_width - box_width) / 2;
  uint16_t starty = (term_height - box_height) / 2;

  keypad(stdscr, TRUE);

  WINDOW *input_box = newwin(box_height, box_width, starty, startx);
  box(input_box, 0, 0);
  mvwprintw(input_box, 0, (box_width - LEN_MESSAGE - 2) / 2, " %s ", message);
  wrefresh(input_box);

  echo();
  mvwgetnstr(input_box, 1, 1, input, max_num_digits);
  noecho();

  delwin(input_box);
}

uint8_t display_popup_menu(const Menu_Entry entries[], uint8_t num_entries) {
  uint16_t menu_width = 0;
  for (size_t i = 0; i < num_entries; i++) {
    uint16_t entry_len = strlen(entries[i].text);
    if (entry_len > menu_width) {
      menu_width = entry_len;
    }
  }
  menu_width += 4;                        // Padding for borders and spacing
  uint16_t menu_height = num_entries + 2; // Entries + borders
  uint16_t startx = (term_width - menu_width) / 2;
  uint16_t starty = (term_height - menu_height) / 2;

  WINDOW *menu_win = newwin(menu_height, menu_width, starty, startx);
  box(menu_win, 0, 0);
  keypad(menu_win, TRUE);

  uint8_t choice = 0;
  bool decision_made = false;
  uint16_t ch;

  while (true) {
    for (size_t i = 0; i < num_entries; i++) {
      if (i == choice) {
        wattron(menu_win, A_REVERSE);
      }
      mvwprintw(menu_win, i + 1, 2, "%s", entries[i].text);
      if (i == choice) {
        wattroff(menu_win, A_REVERSE);
      }
    }
    wrefresh(menu_win);

    ch = wgetch(menu_win);
    switch (ch) {
    case 'k': // Vim up
    case KEY_UP:
      choice = (choice == 0) ? num_entries - 1 : choice - 1;
      break;
    case 'j': // Vim down
    case KEY_DOWN:
      choice = (choice == num_entries - 1) ? 0 : choice + 1;
      break;
    case '\n': // Enter key
    case '\r':
      decision_made = true;
      break;
    case 27:
    case 'q':
      delwin(menu_win);
      return CANCEL;
    }

    if (decision_made) {
      break;
    }
  }

  delwin(menu_win);
  return entries[choice].object;
}

uint32_t get_user_input() {
  char input[MAX_NUM_DIGITS_INTEGER + 2]; // null terminator + newline character
  while (true) {
    display_input_box(
        input, "Number between -2147483648 and 2147483647 or a character:",
        MAX_NUM_DIGITS_INTEGER);

    if (isalpha(input[0])) {
      if (strlen((char *)input) > 1) {
        display_notification_box("Error", "Only one character allowed");
      } else {
        return *(uint32_t *)input;
      }
    } else if (isdigit(input[0]) || input[0] == '-') {
      char *endptr;
      uint64_t tmp_num = strtol((char *)input, &endptr, 10);
      if (*endptr != '\0') {
        const char *str = "Error: Further characters after number: ";
        const char *str2 = proper_str_cat(str, endptr);
        const char *str3 = proper_str_cat(str2, "\n");
        fprintf(stderr, "%s", str3);
      } else if ((int64_t)tmp_num < INT32_MIN || (int64_t)tmp_num > INT32_MAX) {
        fprintf(stderr, "Error: Number out of range, must be between "
                        "-2147483648 and 2147483647\n");
      } else {
        return tmp_num;
      }
    } else {
      display_notification_box("Error", "Invalid input");
    }
  }
}

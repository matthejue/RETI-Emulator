#include "../include/input_output.h"
#include "../include/core_debug.h"
#include "../include/parse/parse_args.h"
#include "../include/tui.h"
#include "../include/utils.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char *abort_hint = "(abort: 'q' or 'esc')";

bool display_notification_box_with_action(const char *title,
                                          const char *message, const char key,
                                          void (*action)(void),
                                          void (*action2)(void)) {
  const uint8_t LEN_ERROR = strlen(title);
  const char *press_enter_text =
      strcmp(title, "Assign Watchobject") == 0 ? "Press Enter to close"
                                               : "Press Enter to skip";
  const uint8_t LEN_PRESS_ENTER = strlen(press_enter_text);
  const uint8_t LEN_MESSAGE = strlen(message);
  uint8_t box_width = max(LEN_MESSAGE + 4, LEN_PRESS_ENTER + 4);
  box_width = max(box_width, strlen(abort_hint) + 4);
  uint8_t box_height = 5;
  uint16_t startx = (term_width - box_width) / 2;
  uint16_t starty = (term_height - box_height) / 2;

  WINDOW *notification_box = newwin(box_height, box_width, starty, startx);
  keypad(notification_box, TRUE);
  box(notification_box, 0, 0);
  mvwprintw(notification_box, 0, (box_width - LEN_ERROR - 2) / 2, " %s ",
            title);
  mvwprintw(notification_box, 1, (box_width - LEN_MESSAGE) / 2, "%s", message);
  mvwprintw(notification_box, 2, (box_width - LEN_PRESS_ENTER) / 2,
            "%s", press_enter_text);
  mvwprintw(notification_box, 3, 2, "%s", abort_hint);
  wrefresh(notification_box);

  bool should_cont = true;
  int ch;
  while (true) {
    ch = wgetch(notification_box);
    if (ch == 'q' || ch == 27) {
      should_cont = false;
      break;
    } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
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

  delwin(notification_box);
  draw_boxes();

  return should_cont;
}

bool display_notification_box(const char *title, const char *message) {
  return display_notification_box_with_action(title, message, '\0', NULL, NULL);
}

bool display_input_box(char *input, const char *message,
                       uint8_t max_num_digits) {
  const uint8_t LEN_MESSAGE = strlen(message);
  uint16_t box_width = max(LEN_MESSAGE, strlen(abort_hint)) + 4;
  uint8_t box_height = 4;
  uint16_t startx = (term_width - box_width) / 2;
  uint16_t starty = (term_height - box_height) / 2;

  WINDOW *input_box = newwin(box_height, box_width, starty, startx);
  keypad(input_box, TRUE);
  box(input_box, 0, 0);
  mvwprintw(input_box, 0, (box_width - LEN_MESSAGE - 2) / 2, " %s ", message);
  mvwprintw(input_box, 2, 2, "%s", abort_hint);

  noecho();
  int previous_cursor = curs_set(1);
  size_t len = 0;
  input[0] = '\0';
  bool accepted = false;
  while (true) {
    size_t offset = len > box_width - 3 ? len - (box_width - 3) : 0;
    mvwprintw(input_box, 1, 1, "%*s", box_width - 2, "");
    mvwprintw(input_box, 1, 1, "%s", input + offset);
    wrefresh(input_box);

    int ch = wgetch(input_box);
    if (ch == 27 || (ch == 'q' && input[0] != '\'')) {
      input[0] = '\0';
      break;
    }
    if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
      accepted = true;
      break;
    }
    if (ch == KEY_BACKSPACE || ch == KEY_DC || ch == 127 || ch == '\b') {
      if (len > 0) {
        input[--len] = '\0';
      }
    } else if (ch == 21) { // Clears the input with Ctrl+U
      len = 0;
      input[0] = '\0';
    } else if (ch >= 0 && ch <= UCHAR_MAX &&
               isprint((unsigned char)ch) && len < max_num_digits) {
      input[len++] = ch;
      input[len] = '\0';
    }
  }
  if (previous_cursor != ERR) {
    curs_set(previous_cursor);
  }

  werase(input_box);
  wrefresh(input_box);
  delwin(input_box);
  return accepted;
}

uint8_t display_popup_menu(const Menu_Entry entries[], uint8_t num_entries) {
  uint16_t menu_width = 0;
  for (size_t i = 0; i < num_entries; i++) {
    uint16_t entry_len = strlen(entries[i].text);
    if (entry_len > menu_width) {
      menu_width = entry_len;
    }
  }
  menu_width = max(menu_width, strlen(abort_hint));
  menu_width += 4;                        // Padding for borders and spacing
  uint16_t menu_height = num_entries + 3; // Entries, abort hint and borders
  uint16_t startx = (term_width - menu_width) / 2;
  uint16_t starty = (term_height - menu_height) / 2;

  WINDOW *menu_win = newwin(menu_height, menu_width, starty, startx);
  box(menu_win, 0, 0);
  mvwprintw(menu_win, num_entries + 1, 2, "%s", abort_hint);
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
    case KEY_ENTER:
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

bool get_user_input(uint32_t *value) {
  char input[MAX_NUM_DIGITS_INTEGER + 2]; // null terminator + newline character
  while (true) {
    if (!display_input_box(
            input, "Number between -2147483648 and 4294967295 or a character:",
            MAX_NUM_DIGITS_INTEGER)) {
      return false;
    }
    const char *error = NULL;
    char trailing_characters[80];

    if (input[0] == '\'') {
      if (input[1] == '\0') {
        *value = '\'';
        return true;
      }
      if (strlen(input) == 4 && input[1] == '\\' && input[3] == '\'') {
        switch (input[2]) {
        case 'n':
          *value = '\n';
          return true;
        case 't':
          *value = '\t';
          return true;
        case '\\':
          *value = '\\';
          return true;
        case '\'':
          *value = '\'';
          return true;
        default:
          error = "Invalid escape sequence";
        }
      } else if (strlen(input) == 3 && input[2] == '\'') {
        *value = (uint8_t)input[1];
        return true;
      } else {
        error = "Invalid quoted character";
      }
    } else if (isdigit((unsigned char)input[0]) || input[0] == '-') {
      char *endptr;
      errno = 0;
      long long tmp_num = strtoll(input, &endptr, 10);
      if (*endptr != '\0') {
        snprintf(trailing_characters, sizeof(trailing_characters),
                 "Error: Further characters after number: %s", endptr);
        error = trailing_characters;
      } else if (errno == ERANGE || tmp_num < INT32_MIN || tmp_num > UINT32_MAX) {
        error = "Number out of range, must be between "
                "-2147483648 and 4294967295";
      } else {
        *value = (uint32_t)tmp_num;
        return true;
      }
    } else if (strlen((char *)input) == 1 &&
               isprint((unsigned char)input[0]) &&
               !isdigit((unsigned char)input[0])) {
      *value = (uint8_t)input[0];
      return true;
    } else {
      error = "Invalid input";
    }
    if (!display_notification_box("Error", error)) {
      return false;
    }
  }
}

#include "../include/input_output.h"
#include "../include/parse_args.h"
#include "../include/tui.h"
#include "../include/utils.h"
#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void display_notification_box(const char *title, const char *message) {
  const uint8_t LEN_ERROR = strlen(title);
  const uint8_t LEN_PRESS_ENTER = strlen("Press Enter to continue");
  const uint8_t LEN_MESSAGE = strlen(message);
  uint8_t box_width = max(LEN_MESSAGE + 4, LEN_PRESS_ENTER + 4);
  uint8_t box_height = 4;
  uint16_t startx = (term_width - box_width) / 2;
  uint16_t starty = (term_height - box_height) / 2;

  WINDOW *error_box = newwin(box_height, box_width, starty, startx);
  box(error_box, 0, 0);
  mvwprintw(error_box, 0, (box_width - LEN_ERROR - 2) / 2, " %s ", title);
  mvwprintw(error_box, 1, (box_width - LEN_MESSAGE) / 2, "%s", message);
  mvwprintw(error_box, 2, (box_width - LEN_PRESS_ENTER) / 2,
            "Press Enter to continue");
  wrefresh(error_box);
  int ch;
  while ((ch = wgetch(error_box)) != '\n' && ch != '\r') {
    // Wait for Enter key (newline or carriage return)
  }

  delwin(error_box);
  draw_boxes();
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
    }

    if (decision_made) {
      break;
    }
  }

  delwin(menu_win);
  return entries[choice].object;
}

void display_error_notification(const char *message) {
  fprintf(stderr, "%s\n", message);
  printf("Press Enter to continue");
  // wait until the Enter key is pressed
  while (getchar() != '\n') {
  }
  printf("\033[A\033[K\033[A\033[K\033[A\033[K");
  fflush(stdout);
}

void display_input_message(char *input, const char *message,
                           uint8_t max_num_digits) {
  while (true) {
    printf("%s ", message);
    if (fgets((char *)input, max_num_digits + 2, stdin) == NULL) {
      fprintf(stderr, "Error: Couldn't read input\n");
    } else {
      // Find the position of the newline character
      uint8_t idx_of_newline = strcspn((char *)input, "\n");
      // If the newline character is not found, it means the input was too
      // long
      if (input[idx_of_newline] != '\n') {
        // Clear the input buffer
        uint32_t c;
        while ((c = getchar()) != '\n' && c != EOF)
          ;
        display_error_notification("Error: Input too long\n");
      } else {
        // Replace the newline character with a null terminator
        input[idx_of_newline] = '\0';
        return;
      }
    }
  }
}

void ask_for_user_input(char *input, char *message, uint8_t max_num_digits) {
  if (better_debug_tui) {
    display_input_box(input, message, max_num_digits);
  } else {
    display_input_message(input, message, max_num_digits);
  }
}

uint8_t ask_for_user_decision(const Menu_Entry menu_entries[],
                              const Menu_Entry identifier_to_obj[],
                              uint8_t num_entries, char *message,
                              uint8_t max_num_digits) {
  if (better_debug_tui) {
    return display_popup_menu(menu_entries, num_entries);
  } else {
    while (true) {
      char *identifier = malloc(MAX_CHARS_BOX_IDENTIFIER + 1);
      display_input_message(identifier, message, max_num_digits);
      for (uint8_t i = 0; i < num_entries; i++) {
        if (strncmp(identifier, identifier_to_obj[i].text,
                    MAX_CHARS_BOX_IDENTIFIER) == 0) {
          return identifier_to_obj[i].object;
        }
      }
      display_error_notification("Error: Input is not the identifier of any register\n");
    }
  }
}

void display_input_error(const char *message) {
  if (better_debug_tui) {
    display_notification_box("Error", message);
  } else {
    display_error_notification(message);
  }
}

uint32_t get_user_input() {
  char input[MAX_NUM_DIGITS_INTEGER + 2]; // null terminator + newline character
  while (true) {
    ask_for_user_input(
        input, "Number between -2147483648 and 2147483647 or a character:",
        MAX_NUM_DIGITS_INTEGER);

    if (isalpha(input[0])) {
      if (strlen((char *)input) > 1) {
        display_input_error("Error: Only one character allowed");
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
      display_input_error("Error: Invalid input");
    }
  }
}

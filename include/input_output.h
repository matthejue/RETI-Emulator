#include <stdint.h>
#include <stdbool.h>

#ifndef INPUT_OUTPUT_H
#define INPUT_OUTPUT_H

typedef struct {
  const char *text;
  uint32_t object;
} Menu_Entry;

typedef struct {
  uint32_t object;
  const char *text;
} Menu_Entry_Reversed;

#define MAX_NUM_DIGITS_INTEGER 11 // log(2**32, 10) + possible minus sign

#define MAX_CHARS_BOX_IDENTIFIER 2

#define MAX_CHARS_WATCHOBJECT 20

void display_input_message(char *input, const char *message,
                           uint8_t max_num_digits);
void display_error_notification(const char *message);
void ask_for_user_input(char *input, char *message, uint8_t max_num_digits);
uint8_t ask_for_user_decision(const Menu_Entry menu_entries[],
                              const Menu_Entry identifier_to_obj[],
                              uint8_t num_entries, char *message,
                              uint8_t max_num_digits);
void display_input_error(const char *message);

void display_input_box(char *input, const char *message,
                       uint8_t max_num_digits);
uint8_t display_popup_menu(const Menu_Entry entries[], uint8_t num_entries);
void display_notification_box(const char *title, const char *message);
bool display_notification_box_with_action(const char *title,
                                          const char *message, const char key,
                                          void (*action)(void));

#endif // INPUT_OUTPUT_H

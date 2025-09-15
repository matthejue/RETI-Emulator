#include <stdbool.h>
#include <stdint.h>

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

void display_input_box(char *input, const char *message,
                       uint8_t max_num_digits);
uint8_t display_popup_menu(const Menu_Entry entries[], uint8_t num_entries);
void display_notification_box(const char *title, const char *message);
bool display_notification_box_with_action(const char *title,
                                          const char *message, const char key,
                                          void (*action)(void),
                                          void (*action2)(void));

#endif // INPUT_OUTPUT_H

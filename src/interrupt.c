#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include <stdint.h>
#include <stdlib.h>

uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 1000;
bool interrupt_timer_active = false;

bool keypress_interrupt_active = false;
bool keypress_interrupt_activatable = false;

void timer_interrupt_check() {
  if (!interrupt_timer_active) {
    return;
  }
  timer_cnt++;
  if (timer_cnt == interrupt_timer_interval) {
    in.arg8 = device_to_isr[INTERRUPT_TIMER - START_DEVICES];
    update_state(HARDWARE_INTERRUPT);
    bool success = out.retbool1;
    if (success) {
      interrupt_timer_active = false;
      if (debug_mode) {
        draw_tui();
      }
    } else {
      display_notification_box("Error", "Timer Interrupt has lower priority "
                                        "than current hardware interrupt");
    }
    timer_cnt = 0;
  }
}

bool keypress_interrupt_trigger() {
  if (keypress_interrupt_active || !keypress_interrupt_activatable) {
    if (keypress_interrupt_active) {
      display_notification_box("Error",
                               "Interrupt can't be interrupted by interrupt "
                               "that was triggered by same signal");
    }
    if (!keypress_interrupt_activatable) {
      display_notification_box(
          "Error",
          "Keyboard Interrupt has no assigned Interrupt Service Routine");
    }
    return false;
  }
  in.arg8 = device_to_isr[KEYPRESS - START_DEVICES];
  update_state(HARDWARE_INTERRUPT);
  bool success = out.retbool1;
  bool should_cont = out.retbool2;
  if (success) {
    keypress_interrupt_active = true;
    if (exec_every_step) {
      draw_tui();
    }
  } else {
    display_notification_box("Error", "Keypress Interrupt has lower priority "
                                      "than current hardware interrupt");
  }
  return should_cont;
}

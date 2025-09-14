#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/log.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include <stdint.h>

uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 1000;

bool interrupt_timer_active = false;

bool keypress_interrupt_active = false;
bool keypress_interrupt_activatable = false;

bool timer_interrupt_check() {
  if (!interrupt_timer_active) {
    return false;
  }
  timer_cnt++;
  bool success = false;
  if (timer_cnt == interrupt_timer_interval) {
    in.arg8 = device_to_isr[INTERRUPT_TIMER - START_DEVICES];
    update_state(HARDWARE_INTERRUPT);
    success = out.retbool1;
    timer_cnt = 0;
  }
  return success;
}

bool keypress_interrupt_trigger() {
  if (keypress_interrupt_active) {
    display_notification_box("Error",
                             "Interrupt can't be interrupted by interrupt "
                             "that was triggered by same signal");

    return false;
  }
  if (!keypress_interrupt_activatable) {
    display_notification_box(
        "Error",
        "Keyboard Interrupt has no assigned Interrupt Service Routine");
    return false;
  }
  in.arg8 = device_to_isr[KEYPRESS - START_DEVICES];
  update_state(HARDWARE_INTERRUPT);
  bool should_cont = out.retbool2;
  return should_cont;
}

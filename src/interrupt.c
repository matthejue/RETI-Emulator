#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/log.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include <stdio.h>
#include <stdint.h>

uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 10;

bool interrupt_timer_active = false;

bool keypress_interrupt_active = false;
bool keypress_interrupt_activatable = false;

static uint8_t current_keypress_interrupt_action_isr = INVALID_ISR_NUM;

static uint8_t get_default_keypress_action_isr(void) {
  if (isr_of_keypress_interrupt != INVALID_ISR_NUM) {
    return isr_of_keypress_interrupt;
  }
  return isr_of_timer_interrupt;
}

void init_keypress_interrupt_action_isr(void) {
  current_keypress_interrupt_action_isr = get_default_keypress_action_isr();
}

uint8_t get_keypress_interrupt_action_isr(void) {
  return current_keypress_interrupt_action_isr;
}

bool cycle_keypress_interrupt_action_isr(void) {
  if (isr_num == 0) {
    display_notification_box(
        "Error", "No Interrupt Service Routine available for keypress action");
    return false;
  }

  if (current_keypress_interrupt_action_isr == INVALID_ISR_NUM ||
      current_keypress_interrupt_action_isr >= isr_num) {
    current_keypress_interrupt_action_isr = 0;
  } else {
    current_keypress_interrupt_action_isr =
        (current_keypress_interrupt_action_isr + 1) % isr_num;
  }

  return true;
}

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
  uint8_t keypress_action_isr = current_keypress_interrupt_action_isr;

  if (keypress_interrupt_active) {
    display_notification_box("Error",
                             "Interrupt can't be interrupted by interrupt "
                             "that was triggered by same signal");

    return false;
  }
  if (keypress_action_isr == INVALID_ISR_NUM) {
    display_notification_box(
        "Error",
        "Keyboard Interrupt has no assigned Interrupt Service Routine");
    return false;
  }
  in.arg8 = keypress_action_isr;
  update_state(HARDWARE_INTERRUPT);
  bool should_cont = out.retbool2;
  return should_cont;
}

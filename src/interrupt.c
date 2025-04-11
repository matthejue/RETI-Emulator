#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include <stdint.h>

uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 100;
bool interrupt_timer_active = false;

bool keypress_interrupt_active = false;
bool keypress_interrupt_activatable = false;

bool restore_isr_active;
bool restore_step_into_activated;

void step_into_activation() { step_into_activated = true; }

void save_state() {
  restore_isr_active = isr_active;
  restore_step_into_activated = step_into_activated;
}

void timer_interrupt_check() {
  if (!interrupt_timer_active) {
    return;
  }
  timer_cnt++;
  if (timer_cnt == interrupt_timer_interval) {
    if (handle_hardware_interrupt(INTERRUPT_TIMER - START_DEVICES)) {
      is_hardware_interrupt = true;
      interrupt_timer_active = false;
      save_state();
      uint8_t isr = device_to_isr[INTERRUPT_TIMER - START_DEVICES];
      current_isr = isr;
      if (debug_mode) {
        draw_tui();
      }
      if (visibility_condition) {
        display_notification_box_with_action(
            "Interrupt Timer", "Press 's' to enter", 's', step_into_activation);
        isr_active = true;
      }
      write_array(regs, PC, read_array(regs, PC, false) - 1, false);
      setup_interrupt(isr);
    }
    timer_cnt = 0;
  }
}

void step_into_activation_and_draw_tui() { step_into_activated = true; }

bool keypress_interrupt_trigger() {
  if (keypress_interrupt_active || !keypress_interrupt_activatable) {
    return false;
  }
  bool should_cont = false;
  if (handle_hardware_interrupt(KEYPRESS - START_DEVICES)) {
    is_hardware_interrupt = true;
    keypress_interrupt_active = true;
    save_state();
    uint8_t isr = device_to_isr[KEYPRESS - START_DEVICES];
    if (visibility_condition) {
      should_cont = display_notification_box_with_action("Keyboard Interrupt",
                                           "Press 's' to enter", 's',
                                           step_into_activation_and_draw_tui);
      isr_active = true;
    }
    write_array(regs, PC, read_array(regs, PC, false) - 1, false);
    setup_interrupt(isr);
    if (step_into_activated) {
      draw_tui();
    }
  }
  return should_cont;
}

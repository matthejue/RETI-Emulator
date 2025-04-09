#include "../include/interrupt.h"
#include "../include/debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include <stdint.h>

static uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 100;
bool interrupt_timer_active = false;

bool keypress_interrupt_active = false;

void timer_interrupt_check() {
  if (!interrupt_timer_active) {
    return;
  }
  timer_cnt++;
  if (timer_cnt == interrupt_timer_interval) {
    if (handle_hardware_interrupt(INTERRUPT_TIMER - START_DEVICES)) {
      is_hardware_interrupt = true;
      interrupt_timer_active = false;
      uint8_t isr = device_to_isr[INTERRUPT_TIMER - START_DEVICES];
      if (debug_mode) {
        draw_tui();
      }
      if (!isr_active || step_into_activated) {
        display_notification_box("Interrupt Timer",
                                 "Interrupt Timer triggered");
      }
      write_array(regs, PC, read_array(regs, PC, false) - 1, false);
      setup_interrupt(isr);
      current_isr = isr;
    }
    timer_cnt = 0;
  }
}

void keypress_interrupt_trigger() {
  if (!keypress_interrupt_active) {
    return;
  }
  if (handle_hardware_interrupt(KEYPRESS - START_DEVICES)) {
    is_hardware_interrupt = true;
    keypress_interrupt_active = true;
    uint8_t isr = device_to_isr[KEYPRESS - START_DEVICES];
    if (!isr_active || step_into_activated) {
      display_notification_box("Keyboard Interrupt",
                               "Keyboard Interrupt triggered");
    }
    write_array(regs, PC, read_array(regs, PC, false) - 1, false);
    setup_interrupt(isr);
    draw_tui();
    current_isr = isr;
  }
}

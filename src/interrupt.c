#include "../include/interrupt.h"
#include "../include/core_debug.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/log.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include "../include/uart.h"
#include <stdio.h>
#include <stdint.h>

uint32_t timer_cnt = 0;
uint32_t interrupt_timer_interval = 0;

bool interrupt_timer_active = false;

bool custom_interrupt_active = false;
bool custom_interrupt_activatable = false;
static bool uart_interrupt_pending = false;
static uint8_t pending_uart_isr = INVALID_ISR_NUM;

void init_custom_interrupt_action_isr(void) {
  sync_interrupt_controller_from_memory();
}

uint8_t get_custom_interrupt_action_isr(void) {
  sync_interrupt_controller_from_memory();
  return device_to_isr[CUSTOM];
}

bool cycle_custom_interrupt_action_isr(void) {
  if (isr_num == 0) {
    display_notification_box(
        "Error", "No Interrupt Service Routine available for custom action");
    return false;
  }

  uint8_t custom_isr = get_custom_interrupt_action_isr();
  if (custom_isr == INVALID_ISR_NUM || custom_isr >= isr_num) {
    custom_isr = 0;
  } else {
    custom_isr = (custom_isr + 1) % isr_num;
  }
  set_interrupt_device_isr(CUSTOM, custom_isr);

  return true;
}

bool timer_interrupt_check() {
  sync_interrupt_controller_from_memory();
  uint32_t timer_interval =
      read_array(uart, SYSTEM_INFO_TIMER_INTERRUPT_INTERVAL, true);
  if (!interrupt_timer_active || timer_interval == 0) {
    return false;
  }
  timer_cnt++;
  bool success = false;
  if (timer_cnt >= timer_interval) {
    in.arg8 = device_to_isr[INTERRUPT_TIMER];
    update_state(HARDWARE_INTERRUPT);
    success = out.retbool1;
    timer_cnt = 0;
  }
  return success;
}

bool custom_interrupt_trigger() {
  uint8_t custom_action_isr = get_custom_interrupt_action_isr();

  if (custom_interrupt_active) {
    display_notification_box("Error",
                             "Interrupt can't be interrupted by interrupt "
                             "that was triggered by same signal");

    return false;
  }
  if (custom_action_isr == INVALID_ISR_NUM) {
    display_notification_box(
        "Error",
        "Custom Interrupt has no assigned Interrupt Service Routine");
    return false;
  }
  in.arg8 = custom_action_isr;
  update_state(HARDWARE_INTERRUPT);
  bool should_cont = out.retbool2;
  return should_cont;
}

bool uart_interrupt_trigger(uint8_t byte) {
  sync_interrupt_controller_from_memory();
  uint8_t uart_isr = device_to_isr[UART_DEVICE];
  if (uart_interrupt_pending || uart_isr == INVALID_ISR_NUM) {
    return false;
  }

  uart[1] = byte;
  uart[2] |= 0b00000010;
  uart_interrupt_pending = true;
  pending_uart_isr = uart_isr;
  in.arg8 = uart_isr;
  update_state(HARDWARE_INTERRUPT);
  return true;
}

void uart_interrupt_completed(uint8_t isr) {
  if (uart_interrupt_pending && isr == pending_uart_isr) {
    uart_interrupt_pending = false;
    pending_uart_isr = INVALID_ISR_NUM;
  }
}

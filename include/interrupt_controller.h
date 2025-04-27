#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRRUPT_CONTROLLER_H
#define INTERRRUPT_CONTROLLER_H

#define START_DEVICES 0b1000

typedef enum {
  INTERRUPT_TIMER = 0b1000,
  UART_RECIEVE,
  UART_SEND,
  KEYPRESS
} Device;

#define NUM_DEVICES 4

extern uint8_t device_to_isr[NUM_DEVICES];
extern uint8_t *isr_to_prio;

extern uint8_t isr_heap[];

void assign_isr_and_prio(Device device, uint8_t isr, uint8_t priority);
uint8_t handle_next_hardware_interrupt();
bool check_prio_isr(uint8_t isr);

#endif // INTERRRUPT_CONTROLLER_H

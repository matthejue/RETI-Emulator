#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRRUPT_CONTROLLER_H
#define INTERRRUPT_CONTROLLER_H

#define START_DEVICES 0b1000

typedef enum { INTERRUPT_TIMER = 0b1000, UART_RECIEVE, UART_SEND, KEYPRESS } Device;

#define NUM_DEVICES 4

extern uint8_t device_to_isr[NUM_DEVICES];
extern uint8_t *isr_to_prio;

extern int8_t stack_top;

extern uint8_t isr_heap[];

struct prio_isr {
  bool has_higher_prio;
  uint8_t isr;
};

void assign_isr_and_prio(Device device, uint8_t isr, uint8_t priority);
bool handle_hardware_interrupt(uint8_t device);
struct prio_isr handle_next_hardware_interrupt();

#endif // INTERRRUPT_CONTROLLER_H

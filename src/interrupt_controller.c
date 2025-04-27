#include "../include/interrupt_controller.h"
#include "../include/datastructures.h"
#include "../include/interrupt.h"
#include "../include/statemachine.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

uint8_t device_to_isr[NUM_DEVICES];
uint8_t *isr_to_prio = NULL;

int8_t stack_top = -1;
uint8_t isr_heap[HEAP_SIZE];

void assign_isr_and_prio(Device device, uint8_t isr, uint8_t priority) {
  device_to_isr[device - START_DEVICES] = isr;
  isr_to_prio[isr] = priority;
}

uint8_t handle_next_hardware_interrupt() {
  uint8_t isr = pop_highest_prio(isr_heap, isr_to_prio);
  bool *success = NULL;
  update_state_with_io(
      RETURN_FROM_INTERRUPT,
      &(struct StateInput){.arg8 =
                               device_to_isr[INTERRUPT_TIMER - START_DEVICES]},
      &(struct StateOutput){.retbool1 = success, .retbool2 = NULL});
  return *success ? isr : MAX_STACK_SIZE;
}

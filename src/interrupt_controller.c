#include "../include/interrupt_controller.h"
#include "../include/datastructures.h"
#include "../include/interrupt.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define MAX_STACK_SIZE UINT8_MAX

uint8_t device_to_isr[NUM_DEVICES];
uint8_t *isr_to_prio = NULL;

uint8_t isr_priority_stack[MAX_STACK_SIZE];
int8_t stack_top = -1;
uint8_t isr_heap[HEAP_SIZE];

void assign_isr_and_prio(Device device, uint8_t isr, uint8_t priority) {
  device_to_isr[device - START_DEVICES] = isr;
  isr_to_prio[isr] = priority;
}

static bool handle_interrupt(uint8_t isr) {
  uint8_t prio = isr_to_prio[isr];
  if (stack_top == -1 || (prio > isr_priority_stack[stack_top] &&
                          (uint8_t)stack_top < MAX_STACK_SIZE - 1)) {
    isr_priority_stack[++stack_top] = prio;
    return true;
  } else if (heap_size < HEAP_SIZE) {
    isr_heap[heap_size] = isr;
    heapify_up(heap_size, isr_heap, isr_to_prio);
    heap_size++;
  }
  return false;
}

bool handle_hardware_interrupt(uint8_t device) {
  uint8_t isr = device_to_isr[device];
  return handle_interrupt(isr);
}

struct prio_isr handle_next_hardware_interrupt() {
  uint8_t isr = pop_highest_prio(isr_heap, isr_to_prio);
  return (struct prio_isr){.has_higher_prio = handle_interrupt(isr),
                           .isr = isr};
}

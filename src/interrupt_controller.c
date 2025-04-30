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

void assign_isr_and_prio(Device device, uint8_t isr, uint8_t priority) {
  device_to_isr[device - START_DEVICES] = isr;
  isr_to_prio[isr] = priority;
}

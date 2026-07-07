#include "../../include/interrupt_controller.h"
#include "../../include/assemble.h"
#include "../../include/interrupt.h"
#include "../../include/uart.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint8_t device_to_isr[NUM_HARDWARE_INTERRUPT_SIGNAL_LINES];
uint8_t device_to_prio[NUM_HARDWARE_INTERRUPT_SIGNAL_LINES];
uint8_t isr_to_prio[NUM_ISR_SLOTS];

static int parse_device_slot(const char *str) {
  if (strcmp(str, "INTTIMER") == 0) {
    return INTERRUPT_TIMER;
  }
  if (strcmp(str, "CUSTOM") == 0) {
    return CUSTOM;
  }

  char *endptr;
  long value = strtol(str, &endptr, 10);
  if (endptr == str || *endptr != '\0' || value < 0 ||
      value >= NUM_HARDWARE_INTERRUPT_SIGNAL_LINES) {
    return -1;
  }
  return value;
}

void sync_interrupt_controller_from_memory(void) {
  uint8_t previous_timer_isr = isr_of_timer_interrupt;

  memset(isr_to_prio, 0, sizeof(isr_to_prio));
  for (uint8_t device = 0; device < NUM_HARDWARE_INTERRUPT_SIGNAL_LINES;
       device++) {
    uint8_t isr = uart[INTERRUPT_CONTROLLER_ISR_BASE + device];
    uint8_t priority = uart[INTERRUPT_CONTROLLER_PRIO_BASE + device];

    device_to_isr[device] = isr;
    device_to_prio[device] = priority;
    if (isr != INVALID_ISR_NUM) {
      isr_to_prio[isr] = priority;
    }
  }

  isr_of_timer_interrupt = device_to_isr[INTERRUPT_TIMER];
  isr_of_custom_interrupt = device_to_isr[CUSTOM];
  if (isr_of_timer_interrupt == INVALID_ISR_NUM) {
    interrupt_timer_active = false;
  } else if (previous_timer_isr != isr_of_timer_interrupt) {
    interrupt_timer_active = true;
  }
  custom_interrupt_activatable = isr_of_custom_interrupt != INVALID_ISR_NUM;
}

void init_interrupt_controller(void) {
  for (uint8_t device = 0; device < NUM_HARDWARE_INTERRUPT_SIGNAL_LINES;
       device++) {
    uart[INTERRUPT_CONTROLLER_ISR_BASE + device] = INVALID_ISR_NUM;
    uart[INTERRUPT_CONTROLLER_PRIO_BASE + device] = 0;
  }
  sync_interrupt_controller_from_memory();
}

void set_interrupt_device_isr(Hardware_Interrupt_Signal_Line device,
                              uint8_t isr) {
  uart[INTERRUPT_CONTROLLER_ISR_BASE + device] = isr;
  sync_interrupt_controller_from_memory();
}

void load_interrupt_controller_config(const char *path) {
  FILE *file = fopen(path, "r");
  if (!file) {
    perror("Failed to open interrupt-controller config");
    exit(EXIT_FAILURE);
  }

  char line[128];
  uint16_t isr = 0;
  while (isr < NUM_ISR_SLOTS && fgets(line, sizeof(line), file) != NULL) {
    char priority_str[32] = "";
    char device_str[32] = "";
    char *ptr = line;
    while (isspace((unsigned char)*ptr)) {
      ptr++;
    }
    if (*ptr == '\0' || *ptr == '#') {
      continue;
    }

    if (sscanf(ptr, "%31s %31s", priority_str, device_str) >= 1 &&
        strcmp(priority_str, "-") != 0 && strcmp(device_str, "-") != 0 &&
        device_str[0] != '\0') {
      int device = parse_device_slot(device_str);
      if (device >= 0) {
        uart[INTERRUPT_CONTROLLER_ISR_BASE + device] = (uint8_t)isr;
        uart[INTERRUPT_CONTROLLER_PRIO_BASE + device] =
            (uint8_t)strtoul(priority_str, NULL, 10);
      }
    }
    isr++;
  }

  fclose(file);
  sync_interrupt_controller_from_memory();
}

#include "../include/statemachine.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

bool debug_activated = false;

void log_variable(const char *file_name, const char *var_name, uint32_t value) {
  char log_file[256];
  snprintf(log_file, sizeof(log_file), "/tmp/%s", file_name);

  FILE *file = fopen(log_file, "a");
  if (file == NULL) {
    perror("Error opening log file");
    exit(EXIT_FAILURE);
  }
  fprintf(file, "%s: %d\n", var_name, value);
  fclose(file);
}

void log_statemachine(Event event) {
  log_variable("statemachine.log", "event", event);
  log_variable("statemachine.log", "current_state", current_state);
  log_variable("statemachine.log", "si_happened", si_happened);
  log_variable("statemachine.log", "isr_finished", isr_finished);
  log_variable("statemachine.log", "execute_every_step", execute_every_step);
  log_variable("statemachine.log", "started_finish_here", started_finish_here);
  log_variable("statemachine.log", "stopped_exec_steps_here",
               stopped_exec_steps_here);
  log_variable("statemachine.log", "stack_top", stack_top);
  log_variable("statemachine.log", "heap_size", heap_size);
  log_variable("statemachine.log", "current_isr", current_isr);
  log_variable("statemachine.log", "step_into_activated", step_into_activated);

  FILE *file = fopen("/tmp/statemachine.log", "a");
  fprintf(file, "\n");
  fclose(file);
}

void debug() {
  if (debug_activated) {
#ifdef __linux__
    __asm__("int3"); // ../.gdbinit
#endif
  }
}

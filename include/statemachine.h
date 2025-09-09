#include "../include/datastructures.h"

#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  CONTINUE,
  BREAKPOINT_ENCOUNTERED,
  FINALIZE,
  HARDWARE_INTERRUPT,
  RETURN_FROM_INTERRUPT,
  SOFTWARE_INTERRUPT,
  STEP_INTO_ACTION
} Event;

struct StateInput {
  uint8_t arg8;
};
extern struct StateOutput out;

struct StateOutput {
  bool retbool1;
  bool retbool2;
};
extern struct StateInput in;

#define MAX_VAL_ISR UINT8_MAX
#define HEAP_SIZE UINT8_MAX

extern uint8_t stacked_isrs_cnt;

extern bool breakpoint_encountered;
extern bool isr_finished;
extern bool isr_not_step_into;

extern uint8_t finished_isr_here;
extern uint8_t not_stepped_into_isr_here;

extern uint8_t deactivated_keypress_interrupt_here;
extern uint8_t deactivated_timer_interrupt_here;

extern int8_t stack_top;
extern uint8_t heap_size;
extern uint8_t latest_isr;
extern bool step_into_activated;

#define MAX_STACK_SIZE UINT8_MAX
extern uint8_t isr_stack[];
extern uint8_t isr_heap[];

void update_state(Event event);
#endif // STATEMACHINE_H

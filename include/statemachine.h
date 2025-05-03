#include "../include/datastructures.h"

#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum { NORMAL_OPERATION, INTERRUPT_HANDLING } State;

typedef enum {
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

extern State current_state;
extern int8_t started_finish_here;
extern int8_t stopped_exec_steps_here;
extern bool isr_finished;
extern int8_t stack_top;
extern bool execute_every_step;
extern uint8_t not_stepped_into_here;
extern uint8_t heap_size;
extern uint8_t current_isr;
extern bool step_into_activated;
extern bool si_happened;

#define MAX_STACK_SIZE UINT8_MAX
extern uint8_t isr_priority_stack[];
extern uint8_t isr_heap[];

bool check_if_int_i(void);
bool check_prio_heap(void);
void insert_into_heap(uint8_t isr);
bool check_prio_isr(uint8_t isr);
void again_exec_steps_if_finished_here(void);
void handle_next_hi(void);
void error_no_si_inside_interrupt(void);
void error_too_many_hardware_interrupts(void);
void return_from_interrupt(void);
bool setup_hardware_interrupt(uint8_t isr);
void again_exec_steps_if_stopped_here(void);
void stop_exec_every_step();

void update_state(Event event);
#endif // STATEMACHINE_H

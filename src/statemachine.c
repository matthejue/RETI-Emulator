#include "../include/statemachine.h"
#include "../include/datastructures.h"
#include "../include/debug.h"
#include "../include/error.h"
#include "../include/input_output.h"
#include "../include/interpr.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/log.h"
#include "../include/reti.h"
#include "../include/stddef.h"
#include <stdbool.h>
#include <stdlib.h>

uint8_t isr_priority_stack[MAX_STACK_SIZE];
uint8_t isr_heap[HEAP_SIZE];

State current_state = NORMAL_OPERATION;
bool si_happened = false;
bool isr_finished = true;
bool execute_every_step = true;
int8_t started_finish_here;
int8_t stopped_exec_steps_here;
int8_t stack_top = -1;
uint8_t heap_size = 0;
uint8_t current_isr;
bool step_into_activated = false;

struct StateInput in = {.arg8 = MAX_VAL_ISR};

struct StateOutput out = {.retbool1 = false, .retbool2 = false};

void again_exec_steps_if_finished_here(void) {
  if (started_finish_here == stack_top) {
    isr_finished = true;
  }
}

void stop_exec_every_step() {
  if (step_into_activated) {
    step_into_activated = false;
    return;
  }
  stopped_exec_steps_here = stack_top;
  execute_every_step = false;
}

void again_exec_steps_if_stopped_here(void) {
  if (stopped_exec_steps_here == stack_top) {
    execute_every_step = true;
  }
}

void step_into_deactivation() {
  stopped_exec_steps_here = stack_top;
  execute_every_step = false;
}

// void step_into_activation() {
// execute_every_step = true;
// }

bool setup_hardware_interrupt(uint8_t isr) {
  stack_top++;
  isr_priority_stack[stack_top] = isr_to_prio[isr];
  bool should_cont = false;
  if (visibility_condition) {
    should_cont = display_notification_box_with_action(
        "Keyboard Interrupt", "Press 's' to enter", 's', step_into_deactivation,
        NULL);
  }
  write_array(regs, PC, read_array(regs, PC, false) - 1, false);
  setup_interrupt(isr);
  return should_cont;
}

bool check_prio_isr(uint8_t isr) {
  uint8_t prio = isr_to_prio[isr];
  return stack_top == -1 || (prio > isr_priority_stack[stack_top] &&
                             (uint8_t)stack_top < MAX_STACK_SIZE - 1);
}

bool check_prio_heap(void) {
  uint8_t prio = isr_to_prio[isr_heap[0]];
  return stack_top == -1 || (prio > isr_priority_stack[stack_top] &&
                             (uint8_t)stack_top < MAX_STACK_SIZE - 1);
}

void insert_into_heap(uint8_t isr) {
  isr_heap[heap_size] = isr;
  heapify_up(heap_size, isr_heap, isr_to_prio);
  heap_size++;
}

void handle_next_hi(void) {
  uint8_t isr = pop_highest_prio(isr_heap, isr_to_prio);
  setup_hardware_interrupt(isr);
}

void error_no_si_inside_interrupt(void) {
  display_notification_box(
      "Error",
      "Software Interrupt can't be triggered inside another Interrupt");
}

void error_too_many_hardware_interrupts(void) {
  fprintf(stderr,
          "Error: Too many Hardware Interrupts, which shouldn't be possible");
  exit(EXIT_FAILURE);
}

void check_deactivation_interrupt_timer() {
  if (current_isr == isr_of_timer_interrupt) {
    interrupt_timer_active = false;
  }
}

void check_reactivation_interrupt_timer() {
  if (current_isr == isr_of_timer_interrupt) {
    interrupt_timer_active = false;
  }
}

bool check_if_int_i() {
  return machine_to_assembly(read_storage(read_array(regs, PC, false)))->op ==
         INT;
}

// Code generated from statemachine
void update_state(Event event) {
  debug();
  switch (current_state) {
  case INTERRUPT_HANDLING:
    // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = SOFTWARE_INTERRUPT |  |
    // error_no_si_inside_interrupt();)
    if (event == SOFTWARE_INTERRUPT) {
      current_state = INTERRUPT_HANDLING;
      error_no_si_inside_interrupt();
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = FINALIZE |
      // isr_finished | started_finish_here=stack_top; isr_finished=false;)
    } else if (event == FINALIZE && isr_finished) {
      current_state = INTERRUPT_HANDLING;
      started_finish_here = stack_top;
      isr_finished = false;
      // INTERRUPT_HANDLING -> NORMAL_OPERATION (event = RETURN_FROM_INTERRUPT |
      // si_happened && stack_top==-1 && heap_size==0 | si_happened=false;
      // return_from_interrupt(); check_reactivation_interrupt_timer();
      // again_exec_steps_if_finished_here();
      // again_exec_steps_if_stopped_here();)
    } else if (event == RETURN_FROM_INTERRUPT && si_happened &&
               stack_top == -1 && heap_size == 0) {
      current_state = NORMAL_OPERATION;
      si_happened = false;
      return_from_interrupt();
      check_reactivation_interrupt_timer();
      again_exec_steps_if_finished_here();
      again_exec_steps_if_stopped_here();
      // INTERRUPT_HANDLING -> NORMAL_OPERATION (event = RETURN_FROM_INTERRUPT |
      // !si_happened && stack_top==0 && heap_size==0 | return_from_interrupt();
      // check_reactivation_interrupt_timer();
      // again_exec_steps_if_finished_here();
      // again_exec_steps_if_stopped_here(); stack_top--;)
    } else if (event == RETURN_FROM_INTERRUPT && !si_happened &&
               stack_top == 0 && heap_size == 0) {
      current_state = NORMAL_OPERATION;
      return_from_interrupt();
      check_reactivation_interrupt_timer();
      again_exec_steps_if_finished_here();
      again_exec_steps_if_stopped_here();
      stack_top--;
      // INTERRUPT_HANDLING -> NORMAL_OPERATION (event = RETURN_FROM_INTERRUPT |
      // !si_happened && stack_top==-1 && heap_size==0 | heap_size--;
      // return_from_interrupt(); check_reactivation_interrupt_timer();
      // again_exec_steps_if_finished_here();
      // again_exec_steps_if_stopped_here();)
    } else if (event == RETURN_FROM_INTERRUPT && !si_happened &&
               stack_top == -1 && heap_size == 0) {
      current_state = NORMAL_OPERATION;
      heap_size--;
      return_from_interrupt();
      check_reactivation_interrupt_timer();
      again_exec_steps_if_finished_here();
      again_exec_steps_if_stopped_here();
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = RETURN_FROM_INTERRUPT
      // | heap_size>0 && check_prio_heap() | return_from_interrupt();
      // heap_size--; check_reactivation_interrupt_timer();
      // again_exec_steps_if_stopped_here(); handle_next_hi();)
    } else if (event == RETURN_FROM_INTERRUPT && heap_size > 0 &&
               check_prio_heap()) {
      current_state = INTERRUPT_HANDLING;
      return_from_interrupt();
      heap_size--;
      check_reactivation_interrupt_timer();
      again_exec_steps_if_stopped_here();
      handle_next_hi();
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = RETURN_FROM_INTERRUPT
      // | heap_size>0 && !check_prio_heap() | return_from_interrupt();
      // check_reactivation_interrupt_timer();
      // again_exec_steps_if_finished_here();
      // again_exec_steps_if_stopped_here(); stack_top--;)
    } else if (event == RETURN_FROM_INTERRUPT && heap_size > 0 &&
               !check_prio_heap()) {
      current_state = INTERRUPT_HANDLING;
      return_from_interrupt();
      check_reactivation_interrupt_timer();
      again_exec_steps_if_finished_here();
      again_exec_steps_if_stopped_here();
      stack_top--;
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = HARDWARE_INTERRUPT |
      // (out.retbool1=check_prio_isr(in.arg8)) | current_isr=in.arg8;
      // check_deactivation_interrupt_timer();
      // out.retbool2=setup_hardware_interrupt(in.arg8);)
    } else if (event == HARDWARE_INTERRUPT &&
               (out.retbool1 = check_prio_isr(in.arg8))) {
      current_state = INTERRUPT_HANDLING;
      current_isr = in.arg8;
      check_deactivation_interrupt_timer();
      out.retbool2 = setup_hardware_interrupt(in.arg8);
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = HARDWARE_INTERRUPT |
      // !(out.retbool1=check_prio_isr(in.arg8)) && heap_size <= HEAP_SIZE |
      // current_isr=in.arg8; check_deactivation_interrupt_timer();
      // insert_into_heap(in.arg8);)
    } else if (event == HARDWARE_INTERRUPT &&
               !(out.retbool1 = check_prio_isr(in.arg8)) &&
               heap_size <= HEAP_SIZE) {
      current_state = INTERRUPT_HANDLING;
      current_isr = in.arg8;
      check_deactivation_interrupt_timer();
      insert_into_heap(in.arg8);
      // INTERRUPT_HANDLING -> INTERRUPT_HANDLING (event = HARDWARE_INTERRUPT |
      // !(out.retbool1=check_prio_isr(in.arg8)) && heap_size > HEAP_SIZE |
      // error_too_many_hardware_interrupts();)
    } else if (event == HARDWARE_INTERRUPT &&
               !(out.retbool1 = check_prio_isr(in.arg8)) &&
               heap_size > HEAP_SIZE) {
      current_state = INTERRUPT_HANDLING;
      error_too_many_hardware_interrupts();
    }
    break;

  case NORMAL_OPERATION:
    // NORMAL_OPERATION -> INTERRUPT_HANDLING (event = SOFTWARE_INTERRUPT |  |
    // si_happened=true; current_isr=in.arg8; stop_exec_every_step();
    // setup_interrupt(in.arg8);)
    if (event == SOFTWARE_INTERRUPT) {
      current_state = INTERRUPT_HANDLING;
      si_happened = true;
      current_isr = in.arg8;
      stop_exec_every_step();
      setup_interrupt(in.arg8);
      // NORMAL_OPERATION -> NORMAL_OPERATION (event = STEP_INTO_ACTION |
      // (out.retbool1=check_if_int_i()) | step_into_activated=true;)
    } else if (event == STEP_INTO_ACTION && (out.retbool1 = check_if_int_i())) {
      current_state = NORMAL_OPERATION;
      step_into_activated = true;
      // NORMAL_OPERATION -> INTERRUPT_HANDLING (event = HARDWARE_INTERRUPT |
      // (out.retbool1=(stack_top==-1)) | current_isr=in.arg8;
      // check_deactivation_interrupt_timer();
      // out.retbool2=setup_hardware_interrupt(in.arg8);)
    } else if (event == HARDWARE_INTERRUPT &&
               (out.retbool1 = (stack_top == -1))) {
      current_state = INTERRUPT_HANDLING;
      current_isr = in.arg8;
      check_deactivation_interrupt_timer();
      out.retbool2 = setup_hardware_interrupt(in.arg8);
    }
    break;
  }
}

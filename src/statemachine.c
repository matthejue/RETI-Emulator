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

uint8_t stacked_isrs_cnt = 0;

uint8_t isr_stack[MAX_STACK_SIZE];
uint8_t isr_heap[HEAP_SIZE];

bool breakpoint_encountered = true;
bool isr_finished = true;
bool isr_not_step_into = true;

uint8_t finished_isr_here;
uint8_t not_stepped_into_isr_here;

uint8_t deactivated_keypress_interrupt_here;
uint8_t deactivated_timer_interrupt_here;

int8_t stack_top = -1;
uint8_t heap_size = 0;
uint8_t latest_isr;
bool step_into_activated = false;

struct StateInput in = {.arg8 = MAX_VAL_ISR};

struct StateOutput out = {.retbool1 = false, .retbool2 = false};

void decide_if_software_int_skipped() {
  if (step_into_activated) {
    step_into_activated = false;
    return;
  }
  not_stepped_into_isr_here = stacked_isrs_cnt;
  isr_not_step_into = false;
}

void check_not_stepped_into_isr_completed(void) {
  if (not_stepped_into_isr_here == stacked_isrs_cnt) {
    isr_not_step_into = true;
  }
}

bool check_if_int_i() {
  return machine_to_assembly(read_storage(read_array(regs, PC, false)))->op ==
         INT;
}

void check_activation_step_into() {
  if ((out.retbool1 = check_if_int_i())) {
    step_into_activated = true;
  }
}

void check_finished_isr_completed(void) {
  if (finished_isr_here == stacked_isrs_cnt) {
    isr_finished = true;
  }
}

void do_step_into_isr() {
  not_stepped_into_isr_here = stacked_isrs_cnt;
  isr_not_step_into = false;
}
void check_deactivation_keypress_interrupt() {
  if (latest_isr == isr_of_timer_interrupt) {
    deactivated_keypress_interrupt_here = stacked_isrs_cnt;
    keypress_interrupt_active = false;
  }
}

void check_reactivation_keypress_interrupt() {
  if (deactivated_keypress_interrupt_here == stacked_isrs_cnt) {
    keypress_interrupt_active = true;
  }
}

void check_deactivation_interrupt_timer() {
  if (latest_isr == isr_of_timer_interrupt) {
    deactivated_timer_interrupt_here = stacked_isrs_cnt;
    interrupt_timer_active = false;
  }
}

void check_reactivation_interrupt_timer() {
  if (deactivated_timer_interrupt_here == stacked_isrs_cnt) {
    interrupt_timer_active = true;
  }
}

bool setup_hardware_interrupt(uint8_t isr) {
  stack_top++;
  isr_stack[stack_top] = isr;
  bool should_cont = false;

  const char *title = NULL;
  if (isr == isr_of_keypress_interrupt) {
    title = "Keyboard Interrupt";
  } else if (isr == isr_of_timer_interrupt) {
    title = "Timer Interrupt";
  }

  if (visibility_condition && title != NULL) {
    should_cont = display_notification_box_with_action(
        title, "Press 's' to enter", 's', do_step_into_isr, NULL);
  }

  write_array(regs, PC, read_array(regs, PC, false) - 1, false);
  setup_interrupt(isr);
  return should_cont;
}

bool check_prio_isr(uint8_t isr) {
  uint8_t prio_current_isr = isr_to_prio[isr];
  uint8_t prio_isr_stack;
  if (stack_top == -1) {
    prio_isr_stack = 0;
  } else {
    prio_isr_stack = isr_to_prio[isr_stack[stack_top]];
  }
  return prio_current_isr > prio_isr_stack;
}

bool check_prio_heap(void) {
  uint8_t prio_current_isr = isr_to_prio[isr_heap[0]];
  uint8_t prio_isr_stack = isr_to_prio[isr_stack[stack_top]];
  return stack_top == -1 || (prio_current_isr > prio_isr_stack &&
                             (uint8_t)stack_top < MAX_STACK_SIZE - 1);
}

void insert_into_heap(uint8_t isr) {
  isr_heap[heap_size] = isr;
  heapify_up(heap_size, isr_heap, isr_to_prio);
  heap_size++;
}

void handle_next_hi(void) {
  uint8_t isr = pop_highest_prio(isr_heap, isr_to_prio);
  latest_isr = isr;
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

void update_state(Event event) {
  debug();
  switch (event) {
  case SOFTWARE_INTERRUPT:
    stacked_isrs_cnt++;
    decide_if_software_int_skipped();
    setup_interrupt(in.arg8);
    break;
  case STEP_INTO_ACTION:
    check_activation_step_into();
    break;
  case HARDWARE_INTERRUPT:
    stacked_isrs_cnt++;
    check_deactivation_keypress_interrupt();
    check_deactivation_interrupt_timer();
    if ((out.retbool1 = (stack_top == -1))) {
      latest_isr = in.arg8;
      out.retbool2 = setup_hardware_interrupt(in.arg8);
    } else if ((out.retbool1 = check_prio_isr(in.arg8))) {
      latest_isr = in.arg8;
      out.retbool2 = setup_hardware_interrupt(in.arg8);
    } else if (!(out.retbool1 = check_prio_isr(in.arg8))) {
      if (heap_size <= HEAP_SIZE) {
        insert_into_heap(in.arg8);
      } else {
        error_too_many_hardware_interrupts();
      }
    }
    break;
  case CONTINUE:
    breakpoint_encountered = false;
    break;
  case BREAKPOINT_ENCOUNTERED:
    breakpoint_encountered = true;
    break;
  case FINALIZE:
    if (isr_finished) {
      finished_isr_here = stack_top;
      isr_finished = false;
    }
    break;
  case RETURN_FROM_INTERRUPT:
    stacked_isrs_cnt--;
    return_from_interrupt();
    check_reactivation_keypress_interrupt();
    check_reactivation_interrupt_timer();
    check_finished_isr_completed();
    check_not_stepped_into_isr_completed();
    if (heap_size == 0) {
      stack_top--;
      latest_isr = MAX_VAL_ISR;
    } else if (heap_size > 0 && check_prio_heap()) {
      stack_top--;
      heap_size--;
      handle_next_hi();
      latest_isr = MAX_VAL_ISR;
    }
    break;
  default:
    fprintf(stderr, "Error: Unknown event type %d\n", event);
    exit(EXIT_FAILURE);
    break;
  }

  log_statemachine(event);
}

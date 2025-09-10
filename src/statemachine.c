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
int8_t hardware_isr_stack_top = -1;
int8_t is_hardware_int_stack_top = -1;
uint8_t heap_size = 0;

uint8_t is_hardware_int_stack[MAX_STACK_SIZE];
uint8_t hardware_isr_stack[MAX_STACK_SIZE];
uint8_t isr_heap[HEAP_SIZE];

bool breakpoint_encountered = true;
bool isr_finished = true;
bool isr_not_step_into = true;

uint8_t finished_isr_here;
uint8_t not_stepped_into_isr_here;

uint8_t deactivated_keypress_interrupt_here;
uint8_t deactivated_timer_interrupt_here;

uint8_t latest_isr;
bool step_into_activated = false;

struct StateInput in = {.arg8 = MAX_VAL_ISR};

struct StateOutput out = {.retbool1 = false, .retbool2 = false};

void remember_was_hardware_int() {
  is_hardware_int_stack_top++;
  is_hardware_int_stack[is_hardware_int_stack_top] = true;
}

void remember_was_software_int() {
  is_hardware_int_stack_top++;
  is_hardware_int_stack[is_hardware_int_stack_top] = false;
}

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

bool decide_if_int_i() {
  return machine_to_assembly(read_storage(read_array(regs, PC, false)))->op ==
         INT;
}

void check_activation_step_into() {
  if ((out.retbool1 = decide_if_int_i())) {
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

void update_latest_isr(uint8_t isr) { latest_isr = isr; }

void update_hardware_interrupt_stack(uint8_t isr) {
  hardware_isr_stack_top++;
  hardware_isr_stack[hardware_isr_stack_top] = isr;
}

bool setup_hardware_interrupt(uint8_t isr) {
  bool should_cont = false;

  const char *title = NULL;
  if (isr == isr_of_keypress_interrupt) {
    title = "Keyboard Interrupt";
  } else if (isr == isr_of_timer_interrupt) {
    title = "Timer Interrupt";
  } else {
    fprintf(stderr, "Error: unknown interrupt type\n");
    exit(EXIT_FAILURE);
  }

  if (visibility_condition) {
    should_cont = display_notification_box_with_action(
        title, "Press 's' to enter", 's', do_step_into_isr, NULL);
  }

  write_array(regs, PC, read_array(regs, PC, false) - 1, false);
  setup_interrupt(isr);
  out.retbool2 = should_cont;
  return should_cont;
}

void check_hardware_int_completed() {
  if (is_hardware_int_stack[is_hardware_int_stack_top]) {
    is_hardware_int_stack_top--;
    hardware_isr_stack_top--;
  }
}

bool decide_prio_higher_stack(uint8_t isr) {
  if (hardware_isr_stack_top == -1) {
    return true;
  }
  uint8_t prio_current_isr = isr_to_prio[isr];
  uint8_t prio_isr_stack =
      isr_to_prio[hardware_isr_stack[hardware_isr_stack_top]];

  bool success = prio_current_isr > prio_isr_stack;
  out.retbool1 = success;
  return success;
}

bool decide_prio_higher_heap(void) {
  if (heap_size == 0) {
    return false;
  }
  if (hardware_isr_stack_top == -1) {
    return true;
  }
  uint8_t prio_waiting_highest_prio_isr = isr_to_prio[isr_heap[0]];
  uint8_t prio_isr_stack =
      isr_to_prio[hardware_isr_stack[hardware_isr_stack_top]];
  return prio_waiting_highest_prio_isr > prio_isr_stack;
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

void check_heap_size_before_insert_into_heap(uint8_t isr) {
  if (heap_size <= HEAP_SIZE) {
    insert_into_heap(isr);
  } else {
    error_too_many_hardware_interrupts();
  }
}

void update_state(Event event) {
  debug();
  switch (event) {
  case SOFTWARE_INTERRUPT:
    remember_was_software_int();
    decide_if_software_int_skipped();
    setup_interrupt(in.arg8);
    break;
  case STEP_INTO_ACTION:
    check_activation_step_into();
    break;
  case HARDWARE_INTERRUPT:
    if (decide_prio_higher_stack(in.arg8)) {
      update_latest_isr(in.arg8);
      check_deactivation_keypress_interrupt();
      check_deactivation_interrupt_timer();
      update_hardware_interrupt_stack(in.arg8);
      setup_hardware_interrupt(in.arg8);
      remember_was_hardware_int();
    } else { // if (out.retbool1 != check_prio_isr(in.arg8)) {
      check_heap_size_before_insert_into_heap(in.arg8);
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
      finished_isr_here = hardware_isr_stack_top;
      isr_finished = false;
    }
    break;
  case RETURN_FROM_INTERRUPT:
    return_from_interrupt();
    check_reactivation_keypress_interrupt();
    check_reactivation_interrupt_timer();
    check_finished_isr_completed();
    check_not_stepped_into_isr_completed();
    check_hardware_int_completed();
    if (decide_prio_higher_heap()) {
      heap_size--;
      handle_next_hi();
      remember_was_hardware_int();
      // latest_isr = MAX_VAL_ISR; TODO: ist das nötig?
    }
    break;
  default:
    fprintf(stderr, "Error: Unknown event type %d\n", event);
    exit(EXIT_FAILURE);
    break;
  }

  log_statemachine(event);
}

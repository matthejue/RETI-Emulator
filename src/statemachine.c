#include "../include/statemachine.h"
#include "../include/datastructures.h"
#include "../include/error.h"
#include "../include/input_output.h"
#include "../include/interpr.h"
#include "../include/interrupt_controller.h"
#include "../include/reti.h"
#include "../include/stddef.h"
#include "../include/stdlib.h"
#include <stdbool.h>

uint8_t isr_priority_stack[MAX_STACK_SIZE];
uint8_t isr_heap[HEAP_SIZE];

State current_state = NB;
bool breakpoint_encountered;
bool isr_finished;
bool execute_every_step = true;
bool isr_active;
bool si_happened;
uint8_t started_finish_here;
uint8_t stopped_exec_steps_here;
bool interrupt_timer_active;
bool keypress_interrupt_active;
int8_t stack_top;
uint8_t heap_size;
uint8_t current_isr;

void again_exec_steps_if_finished_here(void) {
  if (started_finish_here == stack_top) {
    isr_finished = true;
  }
}

void stop_exec_every_step() {
  stopped_exec_steps_here = stack_top;
  execute_every_step = false;
}

void again_exec_steps_if_stopped_here(void) {
  if (stopped_exec_steps_here == stack_top) {
    execute_every_step = true;
  }
}

void step_into_deactivation() { execute_every_step = false; }

void step_into_activation() { execute_every_step = true; }

bool setup_hardware_interrupt(uint8_t isr) {
  stack_top++;
  isr_priority_stack[stack_top] = isr_to_prio[isr];
  bool should_cont = false;
  if (visibility_condition) {
    isr_active = true;
    should_cont = display_notification_box_with_action(
        "Keyboard Interrupt", "Press 's' to enter", 's', step_into_deactivation,
        step_into_activation);
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

void error_no_si_inside_hi(void) {
  display_notification_box(
      "Error",
      "Software Interrupt can't be triggered inside Hardware Interrupt");
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
  return machine_to_assembly(read_storage(read_array(regs, PC, false)))->op !=
         INT;
}

// Code generated from statemachine
void update_state(Event event) {
    update_state_with_io(event, NULL, NULL);
}

void update_state_with_io(Event event, struct StateInput* in, struct StateOutput* out) {
    switch (current_state) {
        case HI:
            // HI -> HI (event = SOFTWARE_INTERRUPT |  | error_no_si_inside_hi();)
            if (event == SOFTWARE_INTERRUPT) {
                current_state = HI;
                error_no_si_inside_hi();
            }
            // HI -> NB (event = RETURN_FROM_INTERRUPT | !si_happened && stack_top==0 && heap_size==0 | isr_active=false; stack_top--; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT && !si_happened && stack_top==0 && heap_size==0) {
                current_state = NB;
                isr_active=false;
                stack_top--;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // HI -> NB (event = RETURN_FROM_INTERRUPT | !si_happened && stack_top==-1 && heap_size==1 | isr_active=false; heap_size--; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT && !si_happened && stack_top==-1 && heap_size==1) {
                current_state = NB;
                isr_active=false;
                heap_size--;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // HI -> SI (event = RETURN_FROM_INTERRUPT | si_happened && stack_top==0 && heap_size==0 | stack_top--; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT && si_happened && stack_top==0 && heap_size==0) {
                current_state = SI;
                stack_top--;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // HI -> SI (event = RETURN_FROM_INTERRUPT | si_happened && stack_top==-1 && heap_size==1 | heap_size--; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT && si_happened && stack_top==-1 && heap_size==1) {
                current_state = SI;
                heap_size--;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // HI -> HI (event = RETURN_FROM_INTERRUPT | heap_size>1 && check_prio_heap() | return_from_interrupt(); heap_size--; check_reactivation_interrupt_timer(); again_exec_steps_if_stopped_here(); handle_next_hi();)
            if (event == RETURN_FROM_INTERRUPT && heap_size>1 && check_prio_heap()) {
                current_state = HI;
                return_from_interrupt();
                heap_size--;
                check_reactivation_interrupt_timer();
                again_exec_steps_if_stopped_here();
                handle_next_hi();
            }
            // HI -> HI (event = RETURN_FROM_INTERRUPT | heap_size>1 && !check_prio_heap() | stack_top--; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT && heap_size>1 && !check_prio_heap()) {
                current_state = HI;
                stack_top--;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // HI -> HI (event = HARDWARE_INTERRUPT | (out->retbool1=check_prio_isr(in->arg8)) | current_isr=in->arg8; check_deactivation_interrupt_timer(in->arg8); out->retbool2=setup_hardware_interrupt(in->arg8);)
            if (event == HARDWARE_INTERRUPT && (out->retbool1=check_prio_isr(in->arg8))) {
                current_state = HI;
                current_isr=in->arg8;
                check_deactivation_interrupt_timer(in->arg8);
                out->retbool2=setup_hardware_interrupt(in->arg8);
            }
            // HI -> HI (event = HARDWARE_INTERRUPT | !(out->retbool1=check_prio_isr(in->arg8)) && heap_size <= HEAP_SIZE | current_isr=in->arg8; check_deactivation_interrupt_timer(in->arg8); insert_into_heap(in->arg8);)
            if (event == HARDWARE_INTERRUPT && !(out->retbool1=check_prio_isr(in->arg8)) && heap_size <= HEAP_SIZE) {
                current_state = HI;
                current_isr=in->arg8;
                check_deactivation_interrupt_timer(in->arg8);
                insert_into_heap(in->arg8);
            }
            // HI -> HI (event = HARDWARE_INTERRUPT | !(out->retbool1=check_prio_isr(in->arg8)) && heap_size > HEAP_SIZE | error_too_many_hardware_interrupts();)
            if (event == HARDWARE_INTERRUPT && !(out->retbool1=check_prio_isr(in->arg8)) && heap_size > HEAP_SIZE) {
                current_state = HI;
                error_too_many_hardware_interrupts();
            }
            // HI -> HI (event = FINALIZE | isr_finished && isr_active | started_finish_here=stack_top; isr_finished=false;)
            if (event == FINALIZE && isr_finished && isr_active) {
                current_state = HI;
                started_finish_here=stack_top;
                isr_finished=false;
            }
            break;

        case NB:
            // NB -> SI (event = SOFTWARE_INTERRUPT |  | isr_active=true; current_isr=in->arg8; stop_exec_every_step(); si_happened=true; setup_interrupt(in->arg8);)
            if (event == SOFTWARE_INTERRUPT) {
                current_state = SI;
                isr_active=true;
                current_isr=in->arg8;
                stop_exec_every_step();
                si_happened=true;
                setup_interrupt(in->arg8);
            }
            // NB -> HI (event = HARDWARE_INTERRUPT | stack_top==-1 | current_isr=in->arg8; check_deactivation_interrupt_timer(in->arg8); out->retbool2=setup_hardware_interrupt(in->arg8);)
            if (event == HARDWARE_INTERRUPT && stack_top==-1) {
                current_state = HI;
                current_isr=in->arg8;
                check_deactivation_interrupt_timer(in->arg8);
                out->retbool2=setup_hardware_interrupt(in->arg8);
            }
            // NB -> SI (event = STEP_INTO_ACTION | check_if_int_i() | isr_active=true; current_isr=in->arg8; si_happened=true; setup_interrupt(in->arg8);)
            if (event == STEP_INTO_ACTION && check_if_int_i()) {
                current_state = SI;
                isr_active=true;
                current_isr=in->arg8;
                si_happened=true;
                setup_interrupt(in->arg8);
            }
            break;

        case SI:
            // SI -> NB (event = RETURN_FROM_INTERRUPT |  | isr_active=false; si_happened=false; return_from_interrupt(); check_reactivation_interrupt_timer(); again_exec_steps_if_finished_here(); again_exec_steps_if_stopped_here();)
            if (event == RETURN_FROM_INTERRUPT) {
                current_state = NB;
                isr_active=false;
                si_happened=false;
                return_from_interrupt();
                check_reactivation_interrupt_timer();
                again_exec_steps_if_finished_here();
                again_exec_steps_if_stopped_here();
            }
            // SI -> HI (event = HARDWARE_INTERRUPT | stack_top==-1 | current_isr=in->arg8; check_deactivation_interrupt_timer(in->arg8); out->retbool2=setup_hardware_interrupt(in->arg8);)
            if (event == HARDWARE_INTERRUPT && stack_top==-1) {
                current_state = HI;
                current_isr=in->arg8;
                check_deactivation_interrupt_timer(in->arg8);
                out->retbool2=setup_hardware_interrupt(in->arg8);
            }
            break;

    }
}

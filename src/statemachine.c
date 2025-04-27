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

State current_state;
bool breakpoint_encountered;
bool isr_finished;
bool step_into_activated;
bool isr_active;
bool si_happened;
uint8_t finished_here;
uint8_t stepped_into_here;
bool interrupt_timer_active;
bool keypress_interrupt_active;
int8_t stack_top;
uint8_t heap_size;

void finish_if_finished_here(void) {
  if (finished_here == stack_top) {
    isr_finished = true;
  }
}

void step_out_if_stepped_into_here(void) {
  if (stepped_into_here == stack_top) {
    step_into_activated = false;
  }
}

void step_into_deactivation() { step_into_activated = false; }

void step_into_activation() { step_into_activated = true; }

void setup_interrupt_for_hi(uint8_t isr, bool *should_cont) {
  stack_top++;
  isr_priority_stack[stack_top] = isr_to_prio[isr];
  if (visibility_condition) {
    isr_active = true;
    *should_cont = display_notification_box_with_action(
        "Keyboard Interrupt", "Press 's' to enter", 's', step_into_deactivation,
        step_into_activation);
  }
  write_array(regs, PC, read_array(regs, PC, false) - 1, false);
  setup_interrupt(isr);
}

bool check_prio_isr(uint8_t isr) {
  uint8_t prio = isr_to_prio[isr];
  return stack_top == -1 || (prio > isr_priority_stack[stack_top] &&
                             (uint8_t)stack_top < MAX_STACK_SIZE - 1);
}

void insert_into_heap(uint8_t isr) {
  isr_heap[heap_size] = isr;
  heapify_up(heap_size, isr_heap, isr_to_prio);
  heap_size++;
}

void handle_next_hi(void) {}
void no_si_inside_hi_error(void) {}

void check_deactivation_interrupt_timer(uint8_t isr) {
  if (isr == isr_of_timer_interrupt) {
    interrupt_timer_active = false;
  }
}

 void check_activation_interrupt_timer(uint8_t isr) {
  if (isr == isr_of_timer_interrupt) {
    interrupt_timer_active = false;
  }
}

// Code generated from statemachine
void update_state(Event event) {
    update_state_with_io(event, NULL, NULL);
}

void update_state_with_io(Event event, struct StateInput* in, struct StateOutput* out) {
    switch (current_state) {
        case HI:
            // HI -> NB (event = RETURN_FROM_INTERRUPT | !si_happened && stack_top==0 && heap_size==0 | isr_active=false; stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT && !si_happened && stack_top==0 && heap_size==0) {
                current_state = NB;
                isr_active=false;
                stack_top--;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // HI -> HI (event = HARDWARE_INTERRUPT | (out->retbool1=check_prio_isr(in->arg8)) | interrupt_timer_active=false; stack_top++; isr_priority_stack[stack_top]=isr_to_prio[arg]; display_notification_box_with_action(); setup_interrupt(arg);)
            if (event == HARDWARE_INTERRUPT && (out->retbool1=check_prio_isr(in->arg8))) {
                current_state = HI;
                interrupt_timer_active=false;
                stack_top++;
                isr_priority_stack[stack_top]=isr_to_prio[arg];
                display_notification_box_with_action();
                setup_interrupt(arg);
            }
            // HI -> HI (event = RETURN_FROM_INTERRUPT | heap_size>1 && !check_prio_heap() | stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT && heap_size>1 && !check_prio_heap()) {
                current_state = HI;
                stack_top--;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // HI -> SI (event = RETURN_FROM_INTERRUPT | si_happened && stack_top==0 && heap_size==0 | stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT && si_happened && stack_top==0 && heap_size==0) {
                current_state = SI;
                stack_top--;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // HI -> HI (event = HARDWARE_INTERRUPT | !(out->retbool1=check_prio_isr(in->arg8)) && heap_size < HEAP_SIZE | interrupt_timer_active = false; insert_into_heap();)
            if (event == HARDWARE_INTERRUPT && !(out->retbool1=check_prio_isr(in->arg8)) && heap_size < HEAP_SIZE) {
                current_state = HI;
                interrupt_timer_active = false;
                insert_into_heap();
            }
            // HI -> HI (event = RETURN_FROM_INTERRUPT | heap_size>1 && check_prio_heap() | return_from_interrupt(); heap_size--; step_out_if_stepped_into_here(); handle_next_hi();)
            if (event == RETURN_FROM_INTERRUPT && heap_size>1 && check_prio_heap()) {
                current_state = HI;
                return_from_interrupt();
                heap_size--;
                step_out_if_stepped_into_here();
                handle_next_hi();
            }
            // HI -> HI (event = FINALIZE | isr_finished && isr_active | finished_here=stack_top; isr_finished=false;)
            if (event == FINALIZE && isr_finished && isr_active) {
                current_state = HI;
                finished_here=stack_top;
                isr_finished=false;
            }
            // HI -> SI (event = RETURN_FROM_INTERRUPT | si_happened && stack_top==-1 && heap_size==1 | heap_size--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT && si_happened && stack_top==-1 && heap_size==1) {
                current_state = SI;
                heap_size--;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // HI -> NB (event = RETURN_FROM_INTERRUPT | !si_happened && stack_top==-1 && heap_size==1 | isr_active=false; heap_size--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT && !si_happened && stack_top==-1 && heap_size==1) {
                current_state = NB;
                isr_active=false;
                heap_size--;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // HI -> HI (event = SOFTWARE_INTERRUPT |  | interrupt_timer_active = false; no_si_inside_hi_error();)
            if (event == SOFTWARE_INTERRUPT) {
                current_state = HI;
                interrupt_timer_active = false;
                no_si_inside_hi_error();
            }
            break;

        case NB:
            // NB -> SI (event = SOFTWARE_INTERRUPT |  | isr_active=true; si_happened=true; setup_interrupt();)
            if (event == SOFTWARE_INTERRUPT) {
                current_state = SI;
                isr_active=true;
                si_happened=true;
                setup_interrupt();
            }
            // NB -> SI (event = STEP_INTO_ACTION | check_if_int_i() | step_into_activated=true; stepped_into_here=stack_top; isr_active=true; si_happened=true; setup_interrupt();)
            if (event == STEP_INTO_ACTION && check_if_int_i()) {
                current_state = SI;
                step_into_activated=true;
                stepped_into_here=stack_top;
                isr_active=true;
                si_happened=true;
                setup_interrupt();
            }
            // NB -> HI (event = HARDWARE_INTERRUPT | stack_top==-1 | check_deactivation_interrupt_timer(in->arg8); setup_interrupt_for_hi(in->arg8, &out->retbool1);)
            if (event == HARDWARE_INTERRUPT && stack_top==-1) {
                current_state = HI;
                check_deactivation_interrupt_timer(in->arg8);
                setup_interrupt_for_hi(in->arg8, &out->retbool1);
            }
            break;

        case SI:
            // SI -> NB (event = RETURN_FROM_INTERRUPT |  | isr_active=false; si_happened=false; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
            if (event == RETURN_FROM_INTERRUPT) {
                current_state = NB;
                isr_active=false;
                si_happened=false;
                return_from_interrupt();
                finish_if_finished_here();
                step_out_if_stepped_into_here();
            }
            // SI -> HI (event = HARDWARE_INTERRUPT | stack_top==-1 | check_deactivation_interrupt_timer(in->arg8); setup_interrupt_for_hi(in->arg8, &out->retbool1);)
            if (event == HARDWARE_INTERRUPT && stack_top==-1) {
                current_state = HI;
                check_deactivation_interrupt_timer(in->arg8);
                setup_interrupt_for_hi(in->arg8, &out->retbool1);
            }
            break;

    }
}

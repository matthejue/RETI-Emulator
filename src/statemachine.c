#include "../include/statemachine.h"
#include "../include/interrupt_controller.h"
#include "../include/datastructures.h"
#include "../include/interpr.h"
#include "../include/statemachine_actions.h"

State current_state;
bool breakpoint_encountered;
bool isr_finished;
bool step_into_activated;
bool isr_active;
bool si_happened;
uint8_t finished_here;
uint8_t stepped_into_here;

void update_state(Event event) {
  switch (current_state) {
    case HI:
      // HI -> NB (event = RTI | !si_happened && stack_top==0 && heap_size==0 | isr_active=false; stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI && !si_happened && stack_top==0 && heap_size==0) {
        current_state = NB;
        isr_active=false;
        stack_top--;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // HI -> HI (event = INT | check_prio_hi() | stack_top++; display_notification_box_with_action(); setup_interrupt();)
      if (event == INT && check_prio_hi()) {
        current_state = HI;
        stack_top++;
        display_notification_box_with_action();
        setup_interrupt();
      }
      // HI -> HI (event = RTI | heap_size>1 && !check_prio_heap() | stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI && heap_size>1 && !check_prio_heap()) {
        current_state = HI;
        stack_top--;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // HI -> SI (event = RTI | si_happened && stack_top==0 && heap_size==0 | stack_top--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI && si_happened && stack_top==0 && heap_size==0) {
        current_state = SI;
        stack_top--;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // HI -> HI (event = INT | !check_prio_hi() | heap_size++; insert_into_heap();)
      if (event == INT && !check_prio_hi()) {
        current_state = HI;
        heap_size++;
        insert_into_heap();
      }
      // HI -> HI (event = RTI | heap_size>1 && check_prio_heap() | return_from_interrupt(); heap_size--; step_out_if_stepped_into_here(); handle_next_hi();)
      if (event == RTI && heap_size>1 && check_prio_heap()) {
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
      // HI -> SI (event = RTI | si_happened && stack_top==-1 && heap_size==1 | heap_size--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI && si_happened && stack_top==-1 && heap_size==1) {
        current_state = SI;
        heap_size--;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // HI -> NB (event = RTI | !si_happened && stack_top==-1 && heap_size==1 | isr_active=false; heap_size--; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI && !si_happened && stack_top==-1 && heap_size==1) {
        current_state = NB;
        isr_active=false;
        heap_size--;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // HI -> HI (event = INT i |  | no_si_inside_hi_error();)
      if (event == INT_I) {
        current_state = HI;
        no_si_inside_hi_error();
      }
      // HI -> HI (event = INT i | stack_top>-1 | si_happened_here(); setup_interrupt();)
      if (event == INT_I && stack_top>-1) {
        current_state = HI;
        si_happened_here();
        setup_interrupt();
      }
      // HI -> HI (event = RTI | check_if_si_happened_here() | return_from_interrupt();)
      if (event == RTI && check_if_si_happened_here()) {
        current_state = HI;
        return_from_interrupt();
      }
      break;

    case NB:
      // NB -> SI (event = INT_I |  | isr_active=true; si_happened=true; setup_interrupt();)
      if (event == INT_I) {
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
      // NB -> HI (event = INT | stack_top==-1 | isr_active=true; display_notification_box_with_action(); setup_interrupt();)
      if (event == INT && stack_top==-1) {
        current_state = HI;
        isr_active=true;
        display_notification_box_with_action();
        setup_interrupt();
      }
      break;

    case SI:
      // SI -> NB (event = RTI |  | isr_active=false; si_happened=false; return_from_interrupt(); finish_if_finished_here(); step_out_if_stepped_into_here();)
      if (event == RTI) {
        current_state = NB;
        isr_active=false;
        si_happened=false;
        return_from_interrupt();
        finish_if_finished_here();
        step_out_if_stepped_into_here();
      }
      // SI -> HI (event = INT | stack_top==-1 | stack_top++; display_notification_box_with_action(); setup_interrupt();)
      if (event == INT && stack_top==-1) {
        current_state = HI;
        stack_top++;
        display_notification_box_with_action();
        setup_interrupt();
      }
      break;

  }
}

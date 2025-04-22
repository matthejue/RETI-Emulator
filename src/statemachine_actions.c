#include "../include/statemachine_actions.h"
#include "../include/input_output.h"
#include "../include/interrupt_controller.h"
#include "../include/statemachine.h"
#include <stdbool.h>

void finsih_if_finished_here() {
  if (finished_here == stack_top) {
    isr_finished = true;
  }
}
void step_out_if_stepped_into_here() {
  if (stepped_into_here == stack_top) {
    step_into_activated = false;
  }
}
bool check_prio_heap() {}
void insert_into_heap() {}
void handle_next_hi() {}
void no_si_inside_hi_error() {
  display_notification_box(
      "Error", "To maintain efficiency, software interrupts are not permitted "
               "during the processing of hardware interrupts");
}

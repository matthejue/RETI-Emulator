#include "../include/assemble.h"
#include "parse/parse_args.h"
#include "../include/statemachine.h"
#include "../include/uart_terminal.h"

#ifndef INTERPRET_H
#define INTERPRET_H

#define MAX_DIGITS_ADDR_DEC 10

#define visibility_condition                                                \
  debug_mode && !uart_terminal_is_active() && breakpoint_encountered &&       \
      isr_finished && isr_step_into

void interpr_instr(Instruction *assembly_instr);
void interpr_prgrm();
void setup_interrupt(uint32_t isr);
void return_from_interrupt();

#endif // INTERPRET_H

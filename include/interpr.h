#include "../include/assemble.h"
#include "../include/parse_args.h"

#ifndef INTERPRET_H
#define INTERPRET_H

#define MAX_DIGITS_ADDR_DEC 10

extern uint8_t current_isr;

#define visibility_condition debug_mode && breakpoint_encountered && isr_finished && (!isr_active || step_into_activated)

void interpr_instr(Instruction *assembly_instr);
void interpr_prgrm();
void setup_interrupt(uint32_t ivt_table_addr) ;

#endif // INTERPRET_H

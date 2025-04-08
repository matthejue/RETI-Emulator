#include "../include/assemble.h"

#ifndef INTERPRET_H
#define INTERPRET_H

#define MAX_DIGITS_ADDR_DEC 10

extern bool is_hardware_interrupt;

extern uint8_t current_isr;

void interpr_instr(Instruction *assembly_instr);
void interpr_prgrm();
void setup_interrupt(uint32_t ivt_table_addr) ;

#endif // INTERPRET_H

#include "../include/assemble.h"
#include "../include/tui.h"
#include <stdio.h>

#ifndef DEBUG_H
#define DEBUG_H

typedef struct {
  Unique_Opcode value;
  const char *name;
} Mnemonic_to_String;

// typedef enum { REGS, EPROM, UART, SRAM, HDD } MemType;
typedef enum { REGS, EPROM, UART, SRAM_C, SRAM_D, SRAM_S } MemType;

extern bool breakpoint_encountered;
extern bool step_into_activated;
extern bool isr_active;

extern char *eprom_watchobject;
extern char *sram_watchobject_cs;
extern char *sram_watchobject_ds;
extern char *sram_watchobject_stack;

char *read_stdin();
void process_and_print_array(uint32_t *array, size_t length);
char *assembly_to_str(Instruction *instr);
char *mem_value_to_str(int32_t mem_content, bool is_unsigned);

void print_mem_content_with_idx(uint64_t idx, uint32_t mem_content,
                                bool are_unsigned, bool are_instrs,
                                MemType mem_type);
void print_reg_content_with_reg(uint8_t idx, uint32_t mem_content);

void print_array_with_idcs(MemType mem_type, uint8_t length, bool are_instrs);
void print_array_with_idcs_from_to(MemType mem_type, uint64_t start,
                                   uint64_t end, bool are_instrs);

void print_file_with_idcs(MemType mem_type, uint64_t start, uint64_t end,
                          bool are_unsigned, bool are_instrs);
bool draw_tui(void);
void evaluate_keyboard_input(void);
void handle_heading(bool better_debug_tui, bool simple_debug_tui, Box *box,
                    char *format_str, char *watchpoint,
                    uint64_t watchpoint_int);

#endif // DEBUG_H

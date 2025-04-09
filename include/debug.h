#include "../include/assemble.h"
#include "../include/input_output.h"
#include "../include/tui.h"
#include <stdio.h>

#ifndef DEBUG_H
#define DEBUG_H

typedef struct {
  Unique_Opcode value;
  const char *name;
} Mnemonic_to_String;

typedef enum { REGS, EPROM, UART, SRAM_C, SRAM_D, SRAM_S } MemType;

typedef enum {
  REGS_BOX,
  EPROM_BOX,
  UART_BOX,
  SRAM_C_BOX,
  SRAM_D_BOX,
  SRAM_S_BOX,
  CANCEL = 0b11111111,
} BoxIdentifier;

extern char *watchobject_addr;

extern const Menu_Entry box_entries[];

extern const Menu_Entry identifier_to_box[];

extern const uint8_t NUM_BOX_ENTRIES;

extern const Menu_Entry register_entries[];

extern const Menu_Entry identifier_to_register_or_address[];

extern const char *register_or_address_to_identifier[];

extern const uint8_t NUM_REGISTER_ENTRIES;

extern bool breakpoint_encountered;
extern bool isr_finished;
extern bool step_into_activated;
extern bool isr_active;

extern Register eprom_watchobject;
extern Register sram_watchobject_cs;
extern Register sram_watchobject_ds;
extern Register sram_watchobject_stack;

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
void handle_heading(bool legacy_debug_tui, bool simple_debug_tui, Box *box,
                    char *format_str, const char *watchobject,
                    uint64_t watchobject_int);

#endif // DEBUG_H

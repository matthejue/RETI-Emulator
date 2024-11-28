#include "../include/assemble.h"
#include <stdio.h>

#ifndef DEBUG_H
#define DEBUG_H

typedef struct {
  Unique_Opcode value;
  const char *name;
} Mnemonic_to_String;

// typedef enum { REGS, EPROM, UART, SRAM, HDD } MemType;
typedef enum { REGS, EPROM, UART, SRAM, SRAM_C, SRAM_D, SRAM_S } MemType;

extern const uint8_t mem_type_to_constant[];

extern bool breakpoint_encountered;
extern bool step_into_activated;
extern bool isr_active;

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
char **split_string(const char *str, uint8_t *count);
bool draw_tui(void);
void get_user_input(void);

#endif // DEBUG_H

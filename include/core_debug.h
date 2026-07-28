#include "../include/assemble.h"
#include "../include/input_output.h"
#include "../include/parse/parse_sections.h"
#include "../include/tui.h"
#include <stdio.h>

extern char **gargv;

#ifndef CORE_DEBUG_H
#define CORE_DEBUG_H

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

typedef struct {
  Box *box;
  Register watchobject;
  char *watchobject_addr;
  int64_t scroll_offset;
} WatchBox;

extern const Menu_Entry box_entries[];

extern const Menu_Entry identifier_to_box[];

extern const uint8_t NUM_BOX_ENTRIES;

extern const Menu_Entry register_entries[];

extern const Menu_Entry identifier_to_register_or_address[];

extern const char *register_or_address_to_identifier[];

extern const uint8_t NUM_REGISTER_ENTRIES;

extern WatchBox eprom_watchbox;
extern WatchBox sram_c_watchbox;
extern WatchBox sram_d_watchbox;
extern WatchBox sram_s_watchbox;

char *read_stdin();
void process_and_print_array(uint32_t *array, size_t length);
char *assembly_to_str(Instruction *instr);
char *mem_value_to_str(int32_t mem_content, bool is_unsigned);

void print_mem_content_with_idx(uint64_t idx, uint32_t mem_content,
                                bool are_unsigned, bool are_instrs,
                                bool is_ascii, MemType mem_type);
void print_reg_content_with_reg(uint8_t idx, uint32_t mem_content);

void print_array_with_idcs(MemType mem_type, uint8_t length, bool are_instrs);
void print_array_with_idcs_from_to(MemType mem_type, uint64_t start,
                                   uint64_t end, bool are_instrs);

void print_file_with_idcs(MemType mem_type, uint64_t start, uint64_t end,
                          bool are_unsigned, bool are_instrs);
void set_sram_debug_sections(Program_Sections sections);
bool draw_tui(void);
void evaluate_keyboard_input(void);
void poll_running_debug_action(void);
void stop_continuous_execution(void);
void wait_for_tui_quit(void);
void debug(void);
void handle_heading(bool simple_debug_tui, Box *box,
                    char *format_str, const char *watchobject,
                    uint64_t watchobject_int);

#endif // CORE_DEBUG_H

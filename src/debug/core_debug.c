#include "../../include/core_debug.h"
#include "../../include/assemble.h"
#include "../../include/input_output.h"
#include "../../include/interrupt.h"
#include "../../include/interrupt_controller.h"
#include "../../include/log.h"
#include "../../include/parse/parse_args.h"
#include "../../include/parse/parse_instrs.h"
#include "../../include/reti.h"
#include "../../include/snapshot_debug.h"
#include "../../include/source_debug.h"
#include "../../include/special_opts.h"
#include "../../include/statemachine.h"
#include "../../include/tui.h"
#include "../../include/uart.h"
#include "../../include/utils.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

uint8_t radius = 2;

char **gargv;

const Menu_Entry box_entries[] = {
    {"EPROM", EPROM_BOX},
    {"SRAM Codesegment", SRAM_C_BOX},
    {"SRAM Datasegment", SRAM_D_BOX},
    {"SRAM Stack", SRAM_S_BOX},
};

const uint8_t NUM_BOX_ENTRIES = sizeof(box_entries) / sizeof(box_entries[0]);

const Menu_Entry register_entries[] = {
    {"PC", PC},   {"IN1", IN1}, {"IN2", IN2}, {"ACC", ACC},         {"SP", SP},
    {"BAF", BAF}, {"CS", CS},   {"DS", DS},   {"Address", ADDRESS},
};

const Menu_Entry assign_value_register_entries[] = {
    {"PC", PC},   {"IN1", IN1}, {"IN2", IN2}, {"ACC", ACC},
    {"SP", SP},   {"BAF", BAF}, {"CS", CS},   {"DS", DS},
};

const Menu_Entry identifier_to_register_or_address[] = {
    {"PC", PC},   {"IN1", IN1}, {"IN2", IN2}, {"ACC", ACC},   {"SP", SP},
    {"BAF", BAF}, {"CS", CS},   {"DS", DS},   {"A", ADDRESS},
};

const char *register_or_address_to_identifier[] = {
    "PC", "IN1", "IN2", "ACC", "SP", "BAF", "CS", "DS", "Address"};

const uint8_t NUM_REGISTER_ENTRIES =
    sizeof(register_entries) / sizeof(register_entries[0]);
const uint8_t NUM_ASSIGN_VALUE_REGISTER_ENTRIES =
    sizeof(assign_value_register_entries) /
    sizeof(assign_value_register_entries[0]);

const uint8_t LINEWIDTH = 54;

WatchBox eprom_watchbox = {&eprom_box, PC, NULL, 0};
WatchBox sram_c_watchbox = {&sram_c_box, PC, NULL, 0};
WatchBox sram_d_watchbox = {&sram_d_box, DS, NULL, 0};
WatchBox sram_s_watchbox = {&sram_s_box, SP, NULL, 0};

static bool sram_sections_exist = false;
static uint32_t sram_codesegment_start = 0;
static uint32_t sram_interrupt_service_routines_start = 0;
static bool sram_has_interrupt_service_routines_start = false;
typedef enum {
  SRAM_TRANSCODE_NUMERIC,
  SRAM_TRANSCODE_INSTRUCTION,
  SRAM_TRANSCODE_ASCII,
  NUM_SRAM_TRANSCODE_MODES,
} SramTranscodeMode;
static SramTranscodeMode sram_transcode_mode = SRAM_TRANSCODE_NUMERIC;

static void cycle_sram_transcode_mode(void) {
  sram_transcode_mode =
      (SramTranscodeMode)((sram_transcode_mode + 1) % NUM_SRAM_TRANSCODE_MODES);
}

typedef enum {
  PERIPHERY_UART_VIEW,
  PERIPHERY_INTERRUPT_CONTROLLER_VIEW,
  PERIPHERY_SYSTEM_INFO_VIEW,
} Periphery_View;

static BoxIdentifier active_box_identifier = EPROM_BOX;
static Periphery_View periphery_view = PERIPHERY_UART_VIEW;

static const BoxIdentifier focus_order[] = {
    REGS_BOX, EPROM_BOX, UART_BOX, SRAM_C_BOX, SRAM_D_BOX, SRAM_S_BOX};
static const uint8_t NUM_FOCUS_BOXES =
    sizeof(focus_order) / sizeof(focus_order[0]);

WatchBox *get_watchbox(BoxIdentifier box_identifier);

void set_sram_debug_sections(Program_Sections sections) {
  sram_sections_exist = sections.exists;
  sram_codesegment_start = sections.codesegment_start;
  sram_interrupt_service_routines_start =
      sections.interrupt_service_routines_start;
  sram_has_interrupt_service_routines_start =
      sections.has_interrupt_service_routines_start;
}

static bool highlighted_watchobject_idx_valid[] = {
    false, false, false, false, false, false};
static uint64_t highlighted_watchobject_idx[] = {0, 0, 0, 0, 0, 0};

static void clear_watchobject_highlights(void) {
  for (uint8_t i = 0;
       i < sizeof(highlighted_watchobject_idx_valid) /
               sizeof(highlighted_watchobject_idx_valid[0]);
       i++) {
    highlighted_watchobject_idx_valid[i] = false;
  }
}

static void set_watchobject_highlight(MemType mem_type, uint64_t idx) {
  highlighted_watchobject_idx_valid[mem_type] = true;
  highlighted_watchobject_idx[mem_type] = idx;
}

static bool is_watchobject_highlight(MemType mem_type, uint64_t idx) {
  return highlighted_watchobject_idx_valid[mem_type] &&
         highlighted_watchobject_idx[mem_type] == idx;
}

static void reset_all_scroll_offsets(void) {
  eprom_watchbox.scroll_offset = 0;
  sram_c_watchbox.scroll_offset = 0;
  sram_d_watchbox.scroll_offset = 0;
  sram_s_watchbox.scroll_offset = 0;
}

static void reset_active_scroll_offset(void) {
  WatchBox *watchbox = get_watchbox(active_box_identifier);
  if (watchbox != NULL) {
    watchbox->scroll_offset = 0;
  }
}

Mnemonic_to_String opcode_to_mnemonic[] = {
    {ADDI, "ADDI"},     {SUBI, "SUBI"},       {MULTI, "MULTI"},
    {DIVI, "DIVI"},     {MODI, "MODI"},       {OPLUSI, "OPLUSI"},
    {ORI, "ORI"},       {ANDI, "ANDI"},       {ADDR, "ADD"},
    {SUBR, "SUB"},      {MULTR, "MULT"},      {DIVR, "DIV"},
    {MODR, "MOD"},      {OPLUSR, "OPLUS"},    {ORR, "OR"},
    {ANDR, "AND"},      {ADDM, "ADD"},        {SUBM, "SUB"},
    {MULTM, "MULT"},    {DIVM, "DIV"},        {MODM, "MOD"},
    {OPLUSM, "OPLUS"},  {ORM, "OR"},          {ANDM, "AND"},
    {LOAD, "LOAD"},     {LOADIN, "LOADIN"},   {LOADI, "LOADI"},
    {STORE, "STORE"},   {STOREIN, "STOREIN"}, {TSL, "TSL"},
    {MOVE, "MOVE"},
    {JUMPGT, "JUMP>"},  {JUMPEQ, "JUMP=="},   {JUMPEQ, "JUMP="},
    {JUMPGE, "JUMP>="}, {JUMPLT, "JUMP<"},    {JUMPNE, "JUMP!="},
    {JUMPNE, "JUMP<>"}, {JUMPLE, "JUMP<="},   {JUMP, "JUMP"},
    {INT, "INT"},       {RTI, "RTI"},         {NOP, "NOP"}};

char *copy_mnemonic_into_str(char *dest, const uint8_t opcode) {
  strcat(dest, opcode_to_mnemonic[opcode].name);
  return dest + strlen(dest);
}

char *copy_reg_into_str(char *dest, const uint8_t reg) {
  strcat(dest, " ");
  dest = strcat(dest, register_code_to_name[reg]);
  return dest + strlen(dest);
}

char *copy_im_into_str(char *dest, const uint32_t im) {
  strcpy(dest, " ");
  if (binary_mode) {
    sprintf(dest + 1, "%s", int_to_bin_str(im, 22));
  } else {
    sprintf(dest + 1, "%d", im);
  }
  return dest + strlen(dest);
}

char *assembly_to_str(Instruction *instr) {
  char *instr_str;
  if (binary_mode) {
    instr_str = malloc(39); // STOREIN ACC IN2 22bit\0
  } else {
    instr_str = malloc(25); // STOREIN ACC IN2 -2097152\0
  }
  instr_str[0] = '\0';
  char *dest = instr_str;
  for (size_t i = 0;
       i < sizeof(opcode_to_mnemonic) / sizeof(opcode_to_mnemonic[0]); i++) {
    if (opcode_to_mnemonic[i].value == instr->op) {
      dest = copy_mnemonic_into_str(dest, i);
      break;
    }
  }
  if ((ADDI <= instr->op && instr->op <= ANDI) ||
      (ADDM <= instr->op && instr->op <= ANDM)) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_im_into_str(dest, instr->opd2);
  } else if (ADDR <= instr->op && instr->op <= ANDR) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
  } else if (instr->op == LOAD || instr->op == STORE || instr->op == LOADI) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_im_into_str(dest, instr->opd2);
  } else if (instr->op == LOADIN || instr->op == STOREIN || instr->op == TSL) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
    dest = copy_im_into_str(dest, instr->opd3);
  } else if (instr->op == MOVE) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
  } else if ((JUMPGT <= instr->op && instr->op <= JUMP) || instr->op == INT) {
    dest = copy_im_into_str(dest, instr->opd1);
  } else if (instr->op == RTI || instr->op == NOP) {
  } else {
    fprintf(stderr, "Invalid instruction\n");
    exit(EXIT_FAILURE);
  }
  return instr_str;
}

static bool is_exact_opcode(uint8_t op, const Unique_Opcode *opcodes,
                            size_t num_opcodes) {
  for (size_t i = 0; i < num_opcodes; i++) {
    if (op == opcodes[i]) {
      return true;
    }
  }
  return false;
}

static bool machine_word_is_valid_instruction(uint32_t machine_instr) {
  uint8_t mode = machine_instr >> 30;
  if (mode == COMPUTE_M) {
    uint8_t compute_mode = machine_instr >> 25;
    return (ADDI <= compute_mode && compute_mode <= ANDI) ||
           (ADDR <= compute_mode && compute_mode <= ANDR) ||
           (ADDM <= compute_mode && compute_mode <= ANDM);
  }

  if (mode == LOAD_M || mode == STORE_M) {
    static const Unique_Opcode load_store_opcodes[] = {
        LOAD, LOADIN, LOADI, STORE, STOREIN, TSL, MOVE};
    uint8_t load_store_mode = (machine_instr >> 28) << 3;
    return is_exact_opcode(load_store_mode, load_store_opcodes,
                           sizeof(load_store_opcodes) /
                               sizeof(load_store_opcodes[0]));
  }

  static const Unique_Opcode jump_opcodes[] = {
      NOP,    INT,    RTI,    JUMPGT, JUMPEQ, JUMPGE,
      JUMPLT, JUMPNE, JUMPLE, JUMP};
  uint8_t jump_mode = machine_instr >> 25;
  return is_exact_opcode(jump_mode, jump_opcodes,
                         sizeof(jump_opcodes) / sizeof(jump_opcodes[0]));
}

char *mem_value_to_str(int32_t mem_content, bool is_unsigned) {
  char *instr_str = malloc(12); // -2147483649
  if (is_unsigned) {
    sprintf(instr_str, "%u", mem_content);
  } else {
    sprintf(instr_str, "%d", mem_content);
  }
  return instr_str;
}

char *mem_value_to_bin_str(uint32_t mem_content) {
  return int_to_bin_str(mem_content, 32);
}

static char *ascii_value_to_str(uint32_t mem_content) {
  static const char *control_names[] = {
      "NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK", "BEL",
      "BS",  "HT",  "LF",  "VT",  "FF",  "CR",  "SO",  "SI",
      "DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
      "CAN", "EM",  "SUB", "ESC", "FS",  "GS",  "RS",  "US"};

  char *ascii_str = malloc(6);
  if (mem_content < 32) {
    snprintf(ascii_str, 6, "%s", control_names[mem_content]);
  } else if (mem_content == 127) {
    snprintf(ascii_str, 6, "DEL");
  } else if (mem_content == '\'') {
    snprintf(ascii_str, 6, "'\\''");
  } else if (mem_content == '\\') {
    snprintf(ascii_str, 6, "'\\\\'");
  } else {
    snprintf(ascii_str, 6, "'%c'", (char)mem_content);
  }
  return ascii_str;
}

char *reg_to_mem_pntr(uint64_t idx, MemType mem_type) {
  char *active_regs = "";
  bool at_least_one_reg = false;
  for (int i = 0; i < NUM_REGISTERS; i++) {
    uint32_t addr = read_array(regs, i, false);
    uint8_t addr_mem_type = addr >> 30;
    uint32_t addr_idx;
    if (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S) {
      addr_idx = addr & 0x7FFFFFFF;
    } else {
      addr_idx = addr & 0x3FFFFFFF;
    }
    if (((addr_mem_type == 0b11 &&
          (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S)) ||
         (addr_mem_type == 0b10 &&
          (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S)) ||
         (addr_mem_type == 0b01 && mem_type == UART) ||
         (addr_mem_type == 0b00 && mem_type == EPROM)) &&
        addr_idx == idx) {
      active_regs = proper_str_cat(active_regs, " ");
      active_regs = proper_str_cat(active_regs, register_code_to_name[i]);
      at_least_one_reg = true;
    }
  }
  if (at_least_one_reg) {
    return proper_str_cat("<-", active_regs);
  }
  return "";
}

void print_formatted_to_box(const char *format, Box *box, ...);
void assign_watchobject_to_box(WatchBox *watchbox, Register watchobject);
uint64_t determine_watchobject_value(WatchBox *watchbox);
static Box *get_box_for_box_identifier(BoxIdentifier box_identifier);

static void cycle_periphery_view(void) {
  periphery_view = (periphery_view + 1) % 3;
}

static void handle_watchobject_assignment(void) {
  if (active_box_identifier == UART_BOX) {
    cycle_periphery_view();
    draw_tui();
    return;
  }

  WatchBox *watchbox = get_watchbox(active_box_identifier);

  if (watchbox == NULL) {
    display_notification_box(
        "Assign Watchobject",
        "Use Tab/S-Tab to select a scrollable address window.");
    draw_tui();
    return;
  }

  Register watchobject = display_popup_menu(register_entries,
                                            NUM_REGISTER_ENTRIES);
  if (watchobject == CANCEL2) {
    draw_tui();
    return;
  }

  assign_watchobject_to_box(watchbox, watchobject);
}

static Box *get_box_for_box_identifier(BoxIdentifier box_identifier) {
  switch (box_identifier) {
  case REGS_BOX:
    return &regs_box;
  case EPROM_BOX:
    return &eprom_box;
  case UART_BOX:
    return &uart_box;
  case SRAM_C_BOX:
    return &sram_c_box;
  case SRAM_D_BOX:
    return &sram_d_box;
  case SRAM_S_BOX:
    return &sram_s_box;
  default:
    return NULL;
  }
}

static void update_active_box_marker(void) {
  set_tui_active_box(get_box_for_box_identifier(active_box_identifier));
}

static void switch_active_window(int8_t direction) {
  for (uint8_t i = 0; i < NUM_FOCUS_BOXES; i++) {
    if (focus_order[i] == active_box_identifier) {
      uint8_t next = (i + NUM_FOCUS_BOXES + direction) % NUM_FOCUS_BOXES;
      active_box_identifier = focus_order[next];
      update_active_box_marker();
      draw_tui();
      return;
    }
  }

  active_box_identifier = EPROM_BOX;
  update_active_box_marker();
  draw_tui();
}

static Box *get_box_for_mem_type(MemType mem_type) {
  switch (mem_type) {
  case REGS:
    return &regs_box;
  case EPROM:
    return &eprom_box;
  case UART:
    return &uart_box;
  case SRAM_C:
    return &sram_c_box;
  case SRAM_D:
    return &sram_d_box;
  case SRAM_S:
    return &sram_s_box;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

static bool comment_matches_mem_type(Source_Comment *comment, MemType mem_type) {
  if (comment->target == COMMENT_TARGET_EPROM) {
    return mem_type == EPROM;
  }
  return mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S;
}

static bool mem_type_has_instruction_comments(MemType mem_type, uint64_t idx) {
  if (!collect_comments) {
    return false;
  }
  if (mem_type == EPROM) {
    return idx < num_instrs_start_prgrm;
  }
  return idx >= num_instrs_isrs && idx < num_instrs_isrs + num_instrs_prgrm &&
         (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S);
}

static uint32_t wrapped_comment_rows(Box *box, const char *text) {
  int max_comment_len = max(0, box->width - 2);
  if (max_comment_len == 0) {
    return 0;
  }

  size_t text_len = strlen(text);
  return max(1, (text_len + max_comment_len - 1) / max_comment_len);
}

static uint32_t count_comments_for_instruction(MemType mem_type, uint64_t idx,
                                               bool before_instruction) {
  if (!mem_type_has_instruction_comments(mem_type, idx)) {
    return 0;
  }

  uint32_t count = 0;
  Box *box = get_box_for_mem_type(mem_type);
  for (uint32_t i = 0; i < num_source_comments; i++) {
    Source_Comment *comment = &source_comments[i];
    if (comment_matches_mem_type(comment, mem_type) &&
        comment->anchor_idx == idx &&
        comment->display_before_instr == before_instruction) {
      count += wrapped_comment_rows(box, comment->text);
    }
  }
  return count;
}

static uint32_t rendered_rows_for_idx(MemType mem_type, uint64_t idx) {
  return 1 + count_comments_for_instruction(mem_type, idx, true) +
         count_comments_for_instruction(mem_type, idx, false);
}

static uint64_t max_idx_for_mem_type(MemType mem_type) {
  switch (mem_type) {
  case EPROM:
    return EPROM_SIZE - 1;
  case UART:
    return NUM_PERIPHERY_ADDRESSES - 1;
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    return sram_size - 1;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

static MemType mem_type_for_box_identifier(BoxIdentifier box_identifier) {
  switch (box_identifier) {
  case UART_BOX:
    return UART;
  case EPROM_BOX:
    return EPROM;
  case SRAM_C_BOX:
    return SRAM_C;
  case SRAM_D_BOX:
    return SRAM_D;
  case SRAM_S_BOX:
    return SRAM_S;
  default:
    return REGS;
  }
}

static bool raw_watchobject_has_address_space(WatchBox *watchbox,
                                              MemType mem_type,
                                              uint64_t raw_watchobject,
                                              uint64_t *idx) {
  switch (mem_type) {
  case EPROM:
    if (raw_watchobject & 0xC0000000) {
      return false;
    }
    *idx = raw_watchobject;
    return true;
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    if (watchbox->watchobject == ADDRESS) {
      *idx = raw_watchobject & 0x7FFFFFFF;
      return true;
    }
    if (!(raw_watchobject & 0x80000000)) {
      return false;
    }
    *idx = raw_watchobject & 0x7FFFFFFF;
    return true;
  case UART:
    if (watchbox->watchobject == ADDRESS) {
      *idx = raw_watchobject;
      return true;
    }
    if ((raw_watchobject >> 30) != UART_CONST) {
      return false;
    }
    *idx = raw_watchobject & 0x3FFFFFFF;
    return true;
  default:
    return false;
  }
}

static void determine_visible_range(MemType mem_type, uint64_t watch_idx,
                                    uint16_t max_rows, uint64_t *start,
                                    uint64_t *end) {
  uint64_t min_idx = 0;
  uint64_t max_idx = max_idx_for_mem_type(mem_type);

  *start = watch_idx;
  *end = watch_idx;

  uint32_t rows_before_watch =
      count_comments_for_instruction(mem_type, watch_idx, true);
  uint32_t rows_after_watch =
      count_comments_for_instruction(mem_type, watch_idx, false);
  uint32_t used_rows = rows_before_watch + 1 + rows_after_watch;

  while (used_rows < max_rows && (*start > min_idx || *end < max_idx)) {
    bool can_expand_up = *start > min_idx;
    bool can_expand_down = *end < max_idx;

    if (can_expand_up &&
        (!can_expand_down || rows_before_watch <= rows_after_watch)) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      if (used_rows + added_rows > max_rows) {
        can_expand_up = false;
      } else {
        (*start)--;
        rows_before_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    if (can_expand_down) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *end + 1);
      if (used_rows + added_rows > max_rows) {
        can_expand_down = false;
      } else {
        (*end)++;
        rows_after_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    if (can_expand_up) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      if (used_rows + added_rows <= max_rows) {
        (*start)--;
        rows_before_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    break;
  }

  if (used_rows < max_rows) {
    uint32_t up_overflow = UINT32_MAX;
    uint32_t down_overflow = UINT32_MAX;

    if (*start > min_idx) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      up_overflow = used_rows + added_rows - max_rows;
    }
    if (*end < max_idx) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *end + 1);
      down_overflow = used_rows + added_rows - max_rows;
    }

    if (up_overflow == UINT32_MAX && down_overflow == UINT32_MAX) {
      return;
    }
    if (up_overflow <= down_overflow) {
      (*start)--;
    } else {
      (*end)++;
    }
  }
}

static void determine_visible_range_from_start(MemType mem_type,
                                               uint64_t start,
                                               uint16_t max_rows,
                                               uint64_t *end) {
  uint64_t max_idx = max_idx_for_mem_type(mem_type);
  uint32_t used_rows = 0;
  *end = start;

  for (uint64_t idx = start; idx <= max_idx; idx++) {
    uint32_t rows = rendered_rows_for_idx(mem_type, idx);
    if (used_rows > 0 && used_rows + rows > max_rows) {
      *end = idx;
      break;
    }
    used_rows += rows;
    *end = idx;
    if (idx == max_idx) {
      break;
    }
  }
}

static bool visible_range_for_watchbox(WatchBox *watchbox, MemType mem_type,
                                       uint64_t raw_watchobject,
                                       uint16_t max_rows, uint64_t *start,
                                       uint64_t *end) {
  uint64_t base_idx;
  if (!raw_watchobject_has_address_space(watchbox, mem_type, raw_watchobject,
                                         &base_idx)) {
    return false;
  }

  uint64_t centered_start;
  uint64_t centered_end;
  determine_visible_range(mem_type, base_idx, max_rows, &centered_start,
                          &centered_end);

  int64_t max_idx = (int64_t)max_idx_for_mem_type(mem_type);
  int64_t visible_start = (int64_t)centered_start + watchbox->scroll_offset;
  if (visible_start < 0) {
    visible_start = 0;
  } else if (visible_start > max_idx) {
    visible_start = max_idx;
  }

  *start = (uint64_t)visible_start;
  determine_visible_range_from_start(mem_type, *start, max_rows, end);
  return true;
}

static void scroll_watchbox(WatchBox *watchbox, MemType mem_type,
                            int8_t direction) {
  uint64_t raw_watchobject = determine_watchobject_value(watchbox);
  if (raw_watchobject == UINT64_MAX) {
    return;
  }

  uint64_t base_idx;
  if (!raw_watchobject_has_address_space(watchbox, mem_type, raw_watchobject,
                                         &base_idx)) {
    return;
  }

  uint16_t max_rows = watchbox->box->height - 2;
  uint64_t centered_start;
  uint64_t centered_end;
  determine_visible_range(mem_type, base_idx, max_rows, &centered_start,
                          &centered_end);

  int64_t max_idx = (int64_t)max_idx_for_mem_type(mem_type);
  int64_t visible_start =
      (int64_t)centered_start + watchbox->scroll_offset + direction;
  if (visible_start < 0) {
    visible_start = 0;
  } else if (visible_start > max_idx) {
    visible_start = max_idx;
  }

  watchbox->scroll_offset = visible_start - (int64_t)centered_start;
}

static void scroll_active_window(int8_t direction) {
  if (active_box_identifier == UART_BOX) {
    return;
  }

  WatchBox *watchbox = get_watchbox(active_box_identifier);
  if (watchbox == NULL) {
    return;
  }

  scroll_watchbox(watchbox, mem_type_for_box_identifier(active_box_identifier),
                  direction);
  draw_tui();
}

static char *address_idx_to_string(uint64_t idx) {
  uint8_t len_addr = snprintf(NULL, 0, "%llu", (unsigned long long)idx) + 1;
  char *addr = malloc(len_addr);
  snprintf(addr, len_addr, "%llu", (unsigned long long)idx);
  return addr;
}

static void change_active_watchobject(int8_t direction) {
  if (active_box_identifier == UART_BOX) {
    return;
  }

  WatchBox *watchbox = get_watchbox(active_box_identifier);
  if (watchbox == NULL) {
    display_notification_box(
        "Change Watchobject",
        "Use Tab/S-Tab to select a scrollable address window.");
    draw_tui();
    return;
  }

  if (watchbox->watchobject == ADDRESS) {
    uint64_t watchobject_value = determine_watchobject_value(watchbox);
    if (watchobject_value == UINT64_MAX) {
      return;
    }

    if (direction < 0 && watchobject_value == 0) {
      display_notification_box("Change Watchobject",
                               "Address cannot be decreased below 0.");
      draw_tui();
      return;
    }

    uint64_t updated_value =
        direction > 0 ? watchobject_value + 1 : watchobject_value - 1;
    free(watchbox->watchobject_addr);
    watchbox->watchobject_addr = address_idx_to_string(updated_value);
  } else {
    uint32_t watchobject_value =
        read_array(regs, watchbox->watchobject, false);
    write_array(regs, watchbox->watchobject, watchobject_value + direction,
                false);
  }

  watchbox->scroll_offset = 0;
  draw_tui();
}

static bool assign_value_to_watchobject_mem_cell(WatchBox *watchbox,
                                                 MemType mem_type,
                                                 uint32_t value) {
  uint64_t raw_watchobject = determine_watchobject_value(watchbox);
  if (raw_watchobject == UINT64_MAX) {
    return false;
  }

  uint64_t idx;
  if (!raw_watchobject_has_address_space(watchbox, mem_type, raw_watchobject,
                                         &idx)) {
    display_notification_box(
        "Assign Value",
        "Selected watchobject does not point into this address space.");
    draw_tui();
    return false;
  }

  switch (mem_type) {
  case EPROM:
    if (idx >= num_instrs_start_prgrm) {
      display_notification_box("Assign Value",
                               "EPROM address is outside loaded EPROM.");
      draw_tui();
      return false;
    }
    write_array(eprom, idx, value, false);
    return true;
  case UART:
    if (idx >= NUM_PERIPHERY_ADDRESSES) {
      display_notification_box(
          "Assign Value",
          "Peripheral address is outside the mapped periphery area.");
      draw_tui();
      return false;
    }
    if (idx == SYSTEM_INFO_SRAM_MAX_ADDRESS) {
      display_notification_box("Assign Value",
                               "SRAM max address is read-only.");
      draw_tui();
      return false;
    }
    write_array(uart, idx, value, true);
    return true;
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    if (idx >= sram_size) {
      display_notification_box("Assign Value",
                               "SRAM address is outside configured SRAM.");
      draw_tui();
      return false;
    }
    write_file(sram, idx, value);
    return true;
  default:
    return false;
  }
}

static void handle_value_assignment(void) {
  if (active_box_identifier == REGS_BOX) {
    Register reg =
        display_popup_menu(assign_value_register_entries,
                           NUM_ASSIGN_VALUE_REGISTER_ENTRIES);
    if (reg == CANCEL2) {
      draw_tui();
      return;
    }

    uint32_t value = get_user_input();
    write_array(regs, reg, value, false);
    reset_all_scroll_offsets();
    draw_tui();
    return;
  }
  if (active_box_identifier == UART_BOX) {
    display_notification_box(
        "Assign Value",
        "The UART box has no assignable watchobject.");
    draw_tui();
    return;
  }

  WatchBox *watchbox = get_watchbox(active_box_identifier);
  if (watchbox == NULL) {
    display_notification_box(
        "Assign Value",
        "Use Tab/S-Tab to select Registers or a scrollable address window.");
    draw_tui();
    return;
  }

  uint32_t value = get_user_input();
  if (assign_value_to_watchobject_mem_cell(
          watchbox, mem_type_for_box_identifier(active_box_identifier), value)) {
    reset_all_scroll_offsets();
    draw_tui();
  }
}

static void print_comments_for_instruction(MemType mem_type, uint64_t idx,
                                           bool before_instruction) {
  if (!collect_comments) {
    return;
  }

  Box *box = get_box_for_mem_type(mem_type);
  int max_comment_len = max(0, box->width - 2);
  if (max_comment_len == 0) {
    return;
  }
  for (uint32_t i = 0; i < num_source_comments; i++) {
    Source_Comment *comment = &source_comments[i];
    if (!comment_matches_mem_type(comment, mem_type) ||
        comment->anchor_idx != idx ||
        comment->display_before_instr != before_instruction) {
      continue;
    }
    size_t text_len = strlen(comment->text);
    size_t offset = 0;
    do {
      int chunk_len = min((int64_t)(text_len - offset), max_comment_len);
      int formatted_len = snprintf(NULL, 0, "%-*.*s\n", max_comment_len,
                                   chunk_len, comment->text + offset);
      char *formatted_comment = malloc(formatted_len + 1);
      snprintf(formatted_comment, formatted_len + 1, "%-*.*s\n",
               max_comment_len, chunk_len, comment->text + offset);
      write_text_into_box_with_attr(box, formatted_comment,
                                    COLOR_PAIR(COMMENT_COLOR_PAIR));
      free(formatted_comment);
      offset += chunk_len;
    } while (offset < text_len);
  }
}

void print_formatted_to_box(const char *format, Box *box, ...) {
  va_list args;
  va_start(args, box);

  char *final_str = create_formatted_str(format, args);
  write_text_into_box(box, final_str);

  va_end(args);
}

static void print_uart_byte_for_tui(uint8_t byte) {
  char buffer[6];
  print_formatted_to_box("%s", &uart_box, format_uart_byte(byte, buffer));
}

static size_t uart_meta_data_limit_for_label(const char *label) {
  size_t inner_width = uart_box.width > 2 ? uart_box.width - 2 : 0;
  size_t label_len = strlen(label);
  size_t current_line_space =
      inner_width > label_len ? inner_width - label_len : 0;
  return current_line_space + inner_width;
}

static void print_uart_bytes_for_tui(const uint8_t *bytes, size_t len) {
  for (size_t i = 0; i < len; i++) {
    print_uart_byte_for_tui(bytes[i]);
  }
}

static void print_uart_bytes_limited_for_tui(const uint8_t *bytes, size_t len,
                                             size_t max_chars) {
  size_t chars_written = 0;
  for (size_t i = 0; i < len; i++) {
    char buffer[6];
    const char *formatted_byte = format_uart_byte(bytes[i], buffer);
    size_t formatted_len = strlen(formatted_byte);
    if (chars_written + formatted_len > max_chars) {
      break;
    }

    print_formatted_to_box("%s", &uart_box, formatted_byte);
    chars_written += formatted_len;
  }
}

static void print_full_width_line_to_box_with_attr(Box *box, int attr,
                                                   const char *line) {
  int inner_width = max(0, box->width - 2);
  if (inner_width == 0) {
    return;
  }

  size_t line_len = strlen(line);
  size_t offset = 0;
  do {
    int chunk_len = min((int64_t)(line_len - offset), inner_width);
    int formatted_len = snprintf(NULL, 0, "%-*.*s\n", inner_width, chunk_len,
                                 line + offset);
    char *formatted_line = malloc(formatted_len + 1);
    snprintf(formatted_line, formatted_len + 1, "%-*.*s\n", inner_width,
             chunk_len, line + offset);
    write_text_into_box_with_attr(box, formatted_line, attr);
    free(formatted_line);
    offset += chunk_len;
  } while (offset < line_len);
}

static const char *uart_cell_label(uint64_t idx);

// TODO:: split zwischen mem content und assembly instrs
// TODO:: Unit test dafür und die ganzen idx Funktionen
void print_mem_content_with_idx(uint64_t idx, uint32_t mem_content,
                                bool are_unsigned, bool are_instrs,
                                bool is_ascii, MemType mem_type) {
  char idx_str[20];
  switch (mem_type) {
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    snprintf(
        idx_str, sizeof(idx_str),
        proper_str_cat(
            proper_str_cat("%0", num_digits_for_idx_str(sram_size - 1)), "zu"),
        idx);
    break;
  case EPROM:
    snprintf(idx_str, sizeof(idx_str),
             proper_str_cat(proper_str_cat("%0", num_digits_for_idx_str(
                                                     num_instrs_start_prgrm)),
                            "zu"),
             idx);
    break;
  case UART:
    snprintf(idx_str, sizeof(idx_str),
             proper_str_cat(proper_str_cat("%0", num_digits_for_idx_str(
                                                     NUM_PERIPHERY_ADDRESSES)),
                            "zu"),
             idx);
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
  const char *mem_content_str;
  if (are_instrs && machine_word_is_valid_instruction(mem_content)) {
    mem_content_str = assembly_to_str(machine_to_assembly(mem_content));
  } else if (is_ascii && mem_content <= 127) {
    mem_content_str = ascii_value_to_str(mem_content);
  } else {
    if (binary_mode) {
      mem_content_str = mem_value_to_bin_str(mem_content);
    } else {
      mem_content_str = mem_value_to_str(mem_content, are_unsigned);
    }
  }

  char *reg_to_mem_pntr_str = reg_to_mem_pntr(idx, mem_type);
  Box *box = get_box_for_mem_type(mem_type);
  const char *variable_label =
      (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S)
          ? source_debug_variable_label_for_sram_idx(idx)
          : NULL;
  bool highlight = is_watchobject_highlight(mem_type, idx);
  int highlight_attr = COLOR_PAIR(WATCHOBJECT_COLOR_PAIR);

  if (mem_type == UART) {
    const char *label = uart_cell_label(idx);
    if (highlight) {
      int line_len = snprintf(NULL, 0, "%s: %s%s %s", idx_str,
                              mem_content_str, reg_to_mem_pntr_str, label);
      char *line = malloc(line_len + 1);
      snprintf(line, line_len + 1, "%s: %s%s %s", idx_str, mem_content_str,
               reg_to_mem_pntr_str, label);
      print_full_width_line_to_box_with_attr(box, highlight_attr, line);
      free(line);
    } else {
      print_formatted_to_box("%s: %s%s ", box, idx_str, mem_content_str,
                             reg_to_mem_pntr_str);
      write_text_into_box_with_attr(
          box, label, A_DIM | COLOR_PAIR(DEBUG_VARIABLE_COLOR_PAIR));
      write_text_into_box(box, "\n");
    }
    return;
  }

  if (variable_label == NULL) {
    if (highlight) {
      int line_len = snprintf(NULL, 0, "%s: %s%s", idx_str, mem_content_str,
                              reg_to_mem_pntr_str);
      char *line = malloc(line_len + 1);
      snprintf(line, line_len + 1, "%s: %s%s", idx_str, mem_content_str,
               reg_to_mem_pntr_str);
      print_full_width_line_to_box_with_attr(box, highlight_attr, line);
      free(line);
    } else {
      print_formatted_to_box("%s: %s%s\n", box, idx_str, mem_content_str,
                             reg_to_mem_pntr_str);
    }
    return;
  }

  if (highlight) {
    int line_len = snprintf(NULL, 0, "%s: %s%s %s", idx_str, mem_content_str,
                            reg_to_mem_pntr_str, variable_label);
    char *line = malloc(line_len + 1);
    snprintf(line, line_len + 1, "%s: %s%s %s", idx_str, mem_content_str,
             reg_to_mem_pntr_str, variable_label);
    print_full_width_line_to_box_with_attr(box, highlight_attr, line);
    free(line);
  } else {
    print_formatted_to_box("%s: %s%s ", box, idx_str, mem_content_str,
                           reg_to_mem_pntr_str);
    write_text_into_box_with_attr(
        box, variable_label, A_DIM | COLOR_PAIR(DEBUG_VARIABLE_COLOR_PAIR));
    write_text_into_box(box, "\n");
  }
}

void print_reg_content_with_reg(uint8_t reg_idx, uint32_t mem_content) {
  char reg_str[4];
  snprintf(reg_str, sizeof(reg_str), "%3s", register_code_to_name[reg_idx]);
  const char *mem_content_str_unsigned;
  if (binary_mode) {
    mem_content_str_unsigned = mem_value_to_bin_str(mem_content);
  } else {
    mem_content_str_unsigned = mem_value_to_str(mem_content, true);
  }
  const char *mem_content_str_signed = mem_value_to_str(mem_content, false);

  print_formatted_to_box("%s: %s (%s)\n", &regs_box, reg_str,
                         mem_content_str_unsigned, mem_content_str_signed);
}

static const char *uart_cell_label(uint64_t idx) {
  switch (idx) {
  case 0:
    return "UART send";
  case 1:
    return "UART receive";
  case 2:
    return "UART status";
  case INTERRUPT_CONTROLLER_ISR_BASE + INTERRUPT_TIMER:
    return "timer isr";
  case INTERRUPT_CONTROLLER_ISR_BASE + CUSTOM:
    return "custom isr";
  case INTERRUPT_CONTROLLER_PRIO_BASE + INTERRUPT_TIMER:
    return "timer priority";
  case INTERRUPT_CONTROLLER_PRIO_BASE + CUSTOM:
    return "custom priority";
  case SYSTEM_INFO_SRAM_MAX_ADDRESS:
    return "SRAM max address";
  case SYSTEM_INFO_TIMER_INTERRUPT_INTERVAL:
    return "timer interrupt interval";
  case SYSTEM_INFO_OS_CS:
    return "Kernel CS";
  case SYSTEM_INFO_OS_DS:
    return "Kernel DS";
  default:
    return "peripheral reserved";
  }
}

void print_array_with_idcs(MemType mem_type, uint8_t length, bool are_instrs) {
  print_array_with_idcs_from_to(mem_type, 0, length - 1, are_instrs);
}

void print_array_with_idcs_from_to(MemType mem_type, uint64_t start,
                                   uint64_t end, bool are_instrs) {
  switch (mem_type) {
  case REGS:
    for (uint8_t i = start; i <= end; i++) {
      print_reg_content_with_reg(i, ((uint32_t *)regs)[i]);
    }
    break;
  case EPROM:
    for (uint16_t i = start; i <= end; i++) {
      if (are_instrs) {
        print_comments_for_instruction(EPROM, i, true);
      }
      if (i < num_instrs_start_prgrm) {
        print_mem_content_with_idx(i, ((uint32_t *)eprom)[i], false, are_instrs,
                                   false, EPROM);
      } else {
        print_mem_content_with_idx(i, 0, false, false, false, EPROM);
      }
      if (are_instrs) {
        print_comments_for_instruction(EPROM, i, false);
      }
    }
    break;
  case UART:
    for (uint8_t i = start; i <= end; i++) {
      bool is_system_info_cell = i >= SYSTEM_INFO_BASE;
      print_mem_content_with_idx(i, read_array(uart, i, true),
                                 is_system_info_cell, are_instrs, false, UART);
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

void print_file_with_idcs(MemType mem_type, uint64_t start, uint64_t end,
                          bool are_unsigned, bool are_instrs) {
  switch (mem_type) {
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    for (uint64_t i = start; i <= end; i++) {
      if (are_instrs) {
        print_comments_for_instruction(mem_type, i, true);
      }
      print_mem_content_with_idx(i, read_file(sram, i), are_unsigned,
                                 are_instrs, false, mem_type);
      if (are_instrs) {
        print_comments_for_instruction(mem_type, i, false);
      }
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

uint64_t determine_watchobject_value(WatchBox *watchbox) {
  if (watchbox->watchobject != ADDRESS) {
    return read_array(regs, watchbox->watchobject, false);
  }

  if (watchbox->watchobject_addr == NULL) {
    display_notification_box("Error", "No address configured for this box");
    // shouldn't be possible
    return UINT64_MAX;
  }

  char *endptr;
  uint64_t watchobject_val = strtol(watchbox->watchobject_addr, &endptr, 10);
  if (*endptr != '\0') {
    const char *str = "Error: Invalid register or number: ";
    const char *str2 = proper_str_cat(str, watchbox->watchobject_addr);

    display_notification_box("Error", str2);
    // this value will never be reached
    return UINT64_MAX;
  } else if (watchobject_val < 0 &&
             watchobject_val >
                 UINT64_MAX) { // this should never happen, because the input
                               // already can't be higher than this
    display_notification_box(
        "Error", "Error: Number out of range, must be between 0 and "
                 "18446744073709551615");
    // this value will never be reached
    return UINT64_MAX;
  }

  return watchobject_val;
}

void print_eprom_watchobject(uint64_t eprom_watchobject) {
  uint64_t start;
  uint64_t end;
  if (!visible_range_for_watchbox(&eprom_watchbox, EPROM, eprom_watchobject,
                                  eprom_box.height - 2, &start, &end)) {
    return;
  }

  if (start < num_instrs_start_prgrm) {
    print_array_with_idcs_from_to(EPROM, start,
                                  min(end, num_instrs_start_prgrm - 1), true);
  }
  if (end >= num_instrs_start_prgrm) {
    print_array_with_idcs_from_to(EPROM, max(num_instrs_start_prgrm, start), end,
                                  false);
  }
}

static bool sram_relative_addr_from_register(Register reg, uint64_t *idx) {
  uint32_t addr = read_array(regs, reg, false);
  uint8_t addr_mem_type = addr >> 30;
  if (addr_mem_type != SRAM_CONST && addr_mem_type != 0b11) {
    return false;
  }

  *idx = addr & 0x7FFFFFFF;
  return true;
}

static bool sram_idx_in_static_isr_range(uint64_t idx) {
  return sram_sections_exist && sram_has_interrupt_service_routines_start &&
         sram_interrupt_service_routines_start < sram_codesegment_start &&
         idx >= sram_interrupt_service_routines_start &&
         idx < sram_codesegment_start;
}

static bool sram_idx_in_dynamic_code_range(uint64_t idx) {
  uint64_t code_start;
  uint64_t data_start;
  if (!sram_relative_addr_from_register(CS, &code_start) ||
      !sram_relative_addr_from_register(DS, &data_start) ||
      data_start <= code_start) {
    return false;
  }

  return idx >= code_start && idx < data_start;
}

static bool sram_idx_should_display_as_instruction(uint64_t idx,
                                                   uint32_t mem_content) {
  return (sram_idx_in_static_isr_range(idx) ||
          sram_idx_in_dynamic_code_range(idx)) &&
         machine_word_is_valid_instruction(mem_content);
}

static bool sram_idx_values_are_unsigned(uint64_t idx) {
  uint64_t data_start;
  if (sram_relative_addr_from_register(DS, &data_start) && idx >= data_start) {
    return ds_vals_unsigned;
  }
  return true;
}

static void print_sram_range(MemType mem_type, uint64_t start, uint64_t end) {
  for (uint64_t i = start; i <= end; i++) {
    uint32_t mem_content = read_file(sram, i);
    bool is_code_instr = sram_idx_should_display_as_instruction(i, mem_content);
    bool are_instrs =
        is_code_instr ||
        (sram_transcode_mode == SRAM_TRANSCODE_INSTRUCTION &&
         machine_word_is_valid_instruction(mem_content));
    bool is_ascii = !is_code_instr &&
                    sram_transcode_mode == SRAM_TRANSCODE_ASCII &&
                    mem_content <= 127;
    if (is_code_instr) {
      print_comments_for_instruction(mem_type, i, true);
    }
    print_mem_content_with_idx(i, mem_content, sram_idx_values_are_unsigned(i),
                               are_instrs, is_ascii, mem_type);
    if (is_code_instr) {
      print_comments_for_instruction(mem_type, i, false);
    }
  }
}

void print_sram_watchobject(uint64_t sram_watchobject_x, MemType mem_type) {
  WatchBox *watchbox = NULL;
  switch (mem_type) {
  case SRAM_C:
    watchbox = &sram_c_watchbox;
    break;
  case SRAM_D:
    watchbox = &sram_d_watchbox;
    break;
  case SRAM_S:
    watchbox = &sram_s_watchbox;
    break;
  default:
    return;
  }

  uint64_t start;
  uint64_t end;
  if (!visible_range_for_watchbox(watchbox, mem_type, sram_watchobject_x,
                                  sram_c_box.height - 2, &start, &end)) {
    return;
  }

  print_sram_range(mem_type, start, end);
}

void print_uart_meta_data() {
  print_formatted_to_box("Current send data: ", &uart_box);
  if (current_send_data != NULL) {
    print_uart_bytes_for_tui((uint8_t *)current_send_data,
                             current_send_data_len);
  }
  print_formatted_to_box("\n", &uart_box);
  const char *all_send_label = "All send data: ";
  print_formatted_to_box("%s", &uart_box, all_send_label);
  if (all_send_data != NULL) {
    print_uart_bytes_limited_for_tui(
        (uint8_t *)all_send_data, all_send_data_len,
        uart_meta_data_limit_for_label(all_send_label));
  }
  print_formatted_to_box("\n", &uart_box);
  print_formatted_to_box("Waiting time sending: ", &uart_box);
  print_formatted_to_box("%d\n", &uart_box, sending_waiting_time);
  print_formatted_to_box("Waiting time receiving: ", &uart_box);
  print_formatted_to_box("%d\n", &uart_box, receiving_waiting_time);
  if (receiving_waiting_time > 0) {
    print_formatted_to_box("Current input: ", &uart_box);
    print_uart_byte_for_tui(receive_current_byte);
    print_formatted_to_box("\n", &uart_box);
  } else {
    print_formatted_to_box("Current input:\n", &uart_box);
  }
  const char *remaining_input_label = "Remaining input: ";
  print_formatted_to_box("%s", &uart_box, remaining_input_label);
  if (input_idx < input_len) {
    print_uart_bytes_limited_for_tui(
        uart_input + input_idx, input_len - input_idx,
        uart_meta_data_limit_for_label(remaining_input_label));
  }
  print_formatted_to_box("\n", &uart_box);
}

WatchBox *get_watchbox(BoxIdentifier box_identifier) {
  switch (box_identifier) {
  case EPROM_BOX:
    return &eprom_watchbox;
  case SRAM_C_BOX:
    return &sram_c_watchbox;
  case SRAM_D_BOX:
    return &sram_d_watchbox;
  case SRAM_S_BOX:
    return &sram_s_watchbox;
  default:
    return NULL;
  }
}

char *ask_watchobject_addr(void) {
  char *watchobject_addr = malloc(MAX_CHARS_WATCHOBJECT + 1);
  display_input_box(watchobject_addr, "Enter an address:",
                    MAX_CHARS_WATCHOBJECT);
  return watchobject_addr;
}

void assign_watchobject_to_box(WatchBox *watchbox, Register watchobject) {
  Register previous_watchobject = watchbox->watchobject;
  char *previous_addr = watchbox->watchobject_addr;
  int64_t previous_scroll_offset = watchbox->scroll_offset;

  if (watchobject == ADDRESS) {
    watchbox->watchobject_addr = ask_watchobject_addr();
  }

  watchbox->watchobject = watchobject;
  watchbox->scroll_offset = 0;
  if (draw_tui()) {
    if (watchobject == ADDRESS) {
      free(previous_addr);
    }
    return;
  }

  watchbox->watchobject = previous_watchobject;
  watchbox->scroll_offset = previous_scroll_offset;
  if (watchobject == ADDRESS) {
    free(watchbox->watchobject_addr);
    watchbox->watchobject_addr = previous_addr;
  }
}

static void restart_emulator(void) {
  cleanup_snapshot_debug();
  finalize();
  execvp(gargv[0], gargv);
}

static int read_tui_key(void) {
  int key = getch();
  if (key != 27) {
    return key;
  }

  nodelay(stdscr, TRUE);
  int second = getch();
  int third = getch();
  nodelay(stdscr, FALSE);

  if (second == '[' && third == 'Z') {
    return KEY_BTAB;
  }
  if (third != ERR) {
    ungetch(third);
  }
  if (second != ERR) {
    ungetch(second);
  }
  return key;
}

void evaluate_keyboard_input(void) {
  while (true) {
    int key = read_tui_key();
    if (key == ERR) {
      continue;
    }
    if (key == 'n') {
      reset_all_scroll_offsets();
      return;
    } else if (key == 'c') {
      reset_all_scroll_offsets();
      update_state(CONTINUE);
      return;
    } else if (key == 'r') {
      reset_all_scroll_offsets();
      restart_emulator();
    } else if (key == 's') {
      reset_all_scroll_offsets();
      update_state(STEP_INTO_ACTION);
      bool success = out.retbool1;
      if (success) {
        return;
      }
      draw_tui();
      continue;
    } else if (key == 'f') {
      reset_all_scroll_offsets();
      update_state(FINALIZE);
      bool success = out.retbool1;
      if (success) {
        return;
      }
      draw_tui();
      continue;
    } else if (key == 't') {
      cycle_sram_transcode_mode();
      draw_tui();
      continue;
    } else if (key == 'T') {
      reset_all_scroll_offsets();
      bool success = custom_interrupt_trigger();
      if (success) {
        return;
      }
      draw_tui();
      continue;
    } else if (key == '\t') {
      switch_active_window(1);
      continue;
    } else if (key == KEY_BTAB) {
      switch_active_window(-1);
      continue;
    } else if (key == 'j') {
      scroll_active_window(1);
      continue;
    } else if (key == 'k') {
      scroll_active_window(-1);
      continue;
    } else if (key == 'J') {
      change_active_watchobject(1);
      continue;
    } else if (key == 'K') {
      change_active_watchobject(-1);
      continue;
    } else if (key == 'C') {
      reset_active_scroll_offset();
      draw_tui();
      continue;
    } else if (key == 'A') {
      handle_value_assignment();
      continue;
    } else if (key == 'a') {
      reset_all_scroll_offsets();
      handle_watchobject_assignment();
    } else if (key == 'o') {
      cycle_info_box_page();
      draw_tui();
      continue;
    } else if (key == 'd') {
      reset_all_scroll_offsets();
      if (!start_source_debugger()) {
        display_notification_box("Source Debug Error",
                                 "Failed to start source debugger");
      }
      draw_tui();
      continue;
    } else if (key == 'S' || key == 'R') {
      reset_all_scroll_offsets();
      if (handle_snapshot_debug_key(key)) {
        continue;
      }
      draw_tui();
      continue;
    } else if (key == 'e') {
      reset_all_scroll_offsets();
      if (cycle_custom_interrupt_action_isr()) {
        draw_tui();
      }
      continue;
    } else if (key == 'D') {
      reset_all_scroll_offsets();
      debug_activated = !debug_activated;
      draw_tui();
      continue;
    } else if (key == 'q') {
      reset_all_scroll_offsets();
      cleanup_snapshot_debug();
      finalize();
      exit(EXIT_SUCCESS);
    }
  }
}

void wait_for_tui_quit(void) {
  set_tui_halted_mode(true);

  while (true) {
    update_term_and_box_sizes();
    draw_tui();

    int key = read_tui_key();
    if (key == ERR) {
      continue;
    }

    switch (key) {
    case 'r':
      reset_all_scroll_offsets();
      restart_emulator();
      break;
    case '\t':
      switch_active_window(1);
      continue;
    case KEY_BTAB:
      switch_active_window(-1);
      continue;
    case 'j':
      scroll_active_window(1);
      continue;
    case 'k':
      scroll_active_window(-1);
      continue;
    case 'J':
      change_active_watchobject(1);
      continue;
    case 'K':
      change_active_watchobject(-1);
      continue;
    case 'C':
      reset_active_scroll_offset();
      draw_tui();
      continue;
    case 'A':
      handle_value_assignment();
      continue;
    case 'a':
      reset_all_scroll_offsets();
      handle_watchobject_assignment();
      break;
    case 'o':
      cycle_info_box_page();
      draw_tui();
      continue;
    case 't':
      cycle_sram_transcode_mode();
      draw_tui();
      continue;
    case 'd':
      reset_all_scroll_offsets();
      if (!start_source_debugger()) {
        display_notification_box("Source Debug Error",
                                 "Failed to start source debugger");
      }
      draw_tui();
      continue;
    case 'S':
    case 'R':
      reset_all_scroll_offsets();
      if (handle_snapshot_debug_key((char)key)) {
        continue;
      }
      continue;
    case 'q':
      reset_all_scroll_offsets();
      cleanup_snapshot_debug();
      set_tui_halted_mode(false);
      return;
    default:
      break;
    }
  }
}

void handle_heading(bool simple_debug_tui, Box *box, char *format_str,
                    const char *watchobject, uint64_t watchobject_int) {
  if (simple_debug_tui) {
    box->title = malloc(strlen(format_str) + 1);
    strcpy(box->title, format_str);
  } else {
    uint8_t len_title =
        snprintf(NULL, 0, format_str, watchobject, watchobject_int) + 1;
    box->title = malloc(len_title);
    snprintf(box->title, len_title, format_str, watchobject, watchobject_int);
  }
}

static void print_interrupt_controller_view(void) {
  handle_heading(true, &uart_box, "Interrupt Controller [a: System Info]", "",
                 0);
  print_array_with_idcs_from_to(UART, INTERRUPT_CONTROLLER_ISR_BASE,
                                SYSTEM_INFO_BASE - 1, false);
}

static void print_system_info_view(void) {
  handle_heading(true, &uart_box, "System Info [a: UART]", "", 0);
  print_array_with_idcs_from_to(UART, SYSTEM_INFO_BASE,
                                NUM_PERIPHERY_ADDRESSES - 1, false);
}

bool draw_tui(void) {
  source_debug_update_current_stackframe_function();
  update_active_box_marker();

  uint64_t eprom_watchobject_int =
      determine_watchobject_value(&eprom_watchbox);
  uint64_t sram_watchobject_cs_int =
      determine_watchobject_value(&sram_c_watchbox);
  uint64_t sram_watchobject_ds_int =
      determine_watchobject_value(&sram_d_watchbox);
  uint64_t sram_watchobject_stack_int =
      determine_watchobject_value(&sram_s_watchbox);
  if (eprom_watchobject_int == UINT64_MAX ||
      sram_watchobject_cs_int == UINT64_MAX ||
      sram_watchobject_ds_int == UINT64_MAX ||
      sram_watchobject_stack_int == UINT64_MAX) {
    return false;
  }

  clear_watchobject_highlights();
  uint64_t highlight_idx;
  if (raw_watchobject_has_address_space(&eprom_watchbox, EPROM,
                                        eprom_watchobject_int,
                                        &highlight_idx) &&
      highlight_idx <= max_idx_for_mem_type(EPROM)) {
    set_watchobject_highlight(EPROM, highlight_idx);
  }
  if (raw_watchobject_has_address_space(&sram_c_watchbox, SRAM_C,
                                        sram_watchobject_cs_int,
                                        &highlight_idx) &&
      highlight_idx <= max_idx_for_mem_type(SRAM_C)) {
    set_watchobject_highlight(SRAM_C, highlight_idx);
  }
  if (raw_watchobject_has_address_space(&sram_d_watchbox, SRAM_D,
                                        sram_watchobject_ds_int,
                                        &highlight_idx) &&
      highlight_idx <= max_idx_for_mem_type(SRAM_D)) {
    set_watchobject_highlight(SRAM_D, highlight_idx);
  }
  if (raw_watchobject_has_address_space(&sram_s_watchbox, SRAM_S,
                                        sram_watchobject_stack_int,
                                        &highlight_idx) &&
      highlight_idx <= max_idx_for_mem_type(SRAM_S)) {
    set_watchobject_highlight(SRAM_S, highlight_idx);
  }

  for (int i = 0; i < NUM_BOXES; i++) {
    wclear(boxes[i]->win);
    reset_box_line(boxes[i]);
    if (extended_features) {
      make_unneccessary_spaces_visible(boxes[i]);
    }
  }

  handle_heading(true, &regs_box, "Registers", "", 0);
  print_array_with_idcs(REGS, NUM_REGISTERS, false);

  handle_heading(false, &eprom_box, "EPROM: %s (%lu)",
                 register_or_address_to_identifier[eprom_watchbox.watchobject],
                 eprom_watchobject_int);
  print_eprom_watchobject(eprom_watchobject_int);

  if (periphery_view == PERIPHERY_INTERRUPT_CONTROLLER_VIEW) {
    print_interrupt_controller_view();
  } else if (periphery_view == PERIPHERY_SYSTEM_INFO_VIEW) {
    print_system_info_view();
  } else {
    handle_heading(true, &uart_box, "UART [a: Interrupt Controller]", "", 0);
    print_array_with_idcs_from_to(UART, 0, 2, false);
    print_uart_meta_data();
  }

  // the user shouldn't have to calculate the absolute address for the sram
  sram_watchobject_cs_int =
      sram_watchobject_cs_int +
      ((sram_c_watchbox.watchobject == ADDRESS) ? (uint32_t)(1 << 31) : 0);
  sram_watchobject_ds_int =
      sram_watchobject_ds_int +
      (uint64_t)((sram_d_watchbox.watchobject == ADDRESS) ? (uint32_t)(1 << 31)
                                                          : 0);
  sram_watchobject_stack_int =
      sram_watchobject_stack_int +
      (uint64_t)((sram_s_watchbox.watchobject == ADDRESS)
                     ? (uint32_t)(1 << 31)
                     : 0);

  handle_heading(false, &sram_c_box, "SRAM Codesegment: %s (%lu)",
                 register_or_address_to_identifier[sram_c_watchbox.watchobject],
                 sram_watchobject_cs_int);
  print_sram_watchobject(sram_watchobject_cs_int, SRAM_C);

  handle_heading(false, &sram_d_box, "SRAM Datasegment: %s (%lu)",
                 register_or_address_to_identifier[sram_d_watchbox.watchobject],
                 sram_watchobject_ds_int);
  print_sram_watchobject(sram_watchobject_ds_int, SRAM_D);

  handle_heading(false, &sram_s_box, "SRAM Stack: %s (%lu)",
                 register_or_address_to_identifier[sram_s_watchbox.watchobject],
                 sram_watchobject_stack_int);
  print_sram_watchobject(sram_watchobject_stack_int, SRAM_S);

  draw_boxes();

  return true;
}

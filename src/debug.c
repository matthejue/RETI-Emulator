#include "../include/debug.h"
#include "../include/assemble.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/tui.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include <ctype.h>
#include <limits.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define clrscr() system("cls")
#else
#define clrscr() printf("\e[1;1H\e[2J") // clear sequence for ANSI terminals
#endif

bool breakpoint_encountered = true;
bool step_into_activated = false;
bool isr_active = false;

bool invalid_input = false;

const uint8_t LINEWIDTH = 54;

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
    {STORE, "STORE"},   {STOREIN, "STOREIN"}, {MOVE, "MOVE"},
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
  } else if (instr->op == LOADIN || instr->op == STOREIN) {
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
      active_regs = proper_str_cat(active_regs, register_code_to_name[i]);
      active_regs = proper_str_cat(active_regs, " ");
      at_least_one_reg = true;
    }
  }
  if (at_least_one_reg) {
    return proper_str_cat("<- ", active_regs);
  }
  return "";
}

void print_formatted_to_stdout_or_box(const char *format, Box *box, ...) {
  va_list args;
  va_start(args, box);
  if (better_debug_tui) {
    char *final_str = create_formatted_str(format, args);
    write_text_into_box(box, final_str);
  } else {
    vprintf(format, args);
  }
  va_end(args);
}

// TODO:: split zwischen mem content und assembly instrs
// TODO:: Unit test dafür und die ganzen idx Funktionen
void print_mem_content_with_idx(uint64_t idx, uint32_t mem_content,
                                bool are_unsigned, bool are_instrs,
                                MemType mem_type) {
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
                                                     NUM_UART_ADDRESSES)),
                            "zu"),
             idx);
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
  const char *mem_content_str;
  if (are_instrs) {
    mem_content_str = assembly_to_str(machine_to_assembly(mem_content));
  } else {
    if (binary_mode) {
      mem_content_str = mem_value_to_bin_str(mem_content);
    } else {
      mem_content_str = mem_value_to_str(mem_content, are_unsigned);
    }
  }

  char *reg_to_mem_pntr_str = reg_to_mem_pntr(idx, mem_type);
  Box *box;
  if (better_debug_tui) {
    switch (mem_type) {
    case REGS:
      box = &regs_box;
      break;
    case EPROM:
      box = &eprom_box;
      break;
    case UART:
      box = &uart_box;
      break;
    case SRAM_C:
      box = &sram_c_box;
      break;
    case SRAM_D:
      box = &sram_d_box;
      break;
    case SRAM_S:
      box = &sram_s_box;
      break;
    default:
      fprintf(stderr, "Error: Invalid memory type\n");
      exit(EXIT_FAILURE);
    }
  } else {
    box = NULL;
  }

  print_formatted_to_stdout_or_box("%s: %s%s\n", box, idx_str, mem_content_str,
                                   reg_to_mem_pntr_str);
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

  print_formatted_to_stdout_or_box("%s: %s (%s)\n", &regs_box, reg_str,
                                   mem_content_str_unsigned,
                                   mem_content_str_signed);
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
      if (i < num_instrs_start_prgrm) {
        print_mem_content_with_idx(i, ((uint32_t *)eprom)[i], false, are_instrs,
                                   EPROM);
      } else {
        print_mem_content_with_idx(i, 0, false, false, EPROM);
      }
    }
    break;
  case UART:
    for (uint8_t i = start; i <= end; i++) {
      print_mem_content_with_idx(i, ((uint8_t *)uart)[i], false, are_instrs,
                                 UART);
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
      print_mem_content_with_idx(i, read_file(sram, i), are_unsigned,
                                 are_instrs, mem_type);
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

char **split_string(const char *str, uint8_t *count) {
  char *str_copy = strdup(str);

  char **result = NULL;
  int words = 0;

  char *token = strtok(str_copy, " \t\n");
  while (token != NULL) {
    result = realloc(result, (words + 1) * sizeof(char *));

    result[words] = strdup(token);
    words++;
    token = strtok(NULL, " \t\n");
  }
  *count = words;

  free(str_copy);

  return result;
}

uint64_t determine_watchpoint_value(char *watchpoint_str) {
  for (int i = 0; i < NUM_REGISTERS; i++) {
    if (strcmp(watchpoint_str, register_code_to_name[i]) == 0) {
      return read_array(regs, i, false);
    }
  }

  char *endptr;
  uint64_t watchpoint_val = strtol(watchpoint_str, &endptr, 10);
  if (*endptr != '\0') {
    const char *str = "Error: Invalid register or number: ";
    const char *str2 = proper_str_cat(str, watchpoint_str);
    const char *str3 = proper_str_cat(str2, "\n");
    fprintf(stderr, "%s", str3);
    invalid_input = true;
    // this value will never be reached
    return UINT64_MAX;
  } else if (watchpoint_val < 0 && watchpoint_val > UINT64_MAX) {
    fprintf(stderr, "Error: Number out of range, must be between 0 and "
                    "18446744073709551615\n");
    invalid_input = true;
    // this value will never be reached
    return UINT64_MAX;
  }

  return watchpoint_val;
}

void print_eprom_watchpoint(uint64_t eprom_watchpoint) {
  if (eprom_watchpoint & 0xC0000000) {
    return;
  }
  if (better_debug_tui) {
    radius = (eprom_box.height - 2) / 2;
  }
  uint8_t diameter_adjust_lower = (int64_t)(eprom_watchpoint - radius) < 0
                                      ? -(int64_t)(eprom_watchpoint - radius)
                                      : 0;
  uint8_t diameter_adjust_upper =
      eprom_watchpoint + radius > (sram_size - 1)
          ? (eprom_watchpoint + radius) - (sram_size - 1)
          : 0;
  print_array_with_idcs_from_to(
      EPROM, max(0, eprom_watchpoint - radius - diameter_adjust_upper),
      min(eprom_watchpoint + radius + diameter_adjust_lower,
          num_instrs_start_prgrm - 1),
      true);
  print_array_with_idcs_from_to(
      EPROM,
      max(num_instrs_start_prgrm,
          eprom_watchpoint - radius - diameter_adjust_upper),
      min(eprom_watchpoint + radius + diameter_adjust_lower, EPROM_SIZE - 1),
      false);
}

void print_sram_watchpoint(uint64_t sram_watchpoint_x, MemType mem_type) {
  if (!(sram_watchpoint_x & 0x80000000)) {
    return;
  }
  if (better_debug_tui) {
    radius = (sram_c_box.height - 2) / 2;
  }
  sram_watchpoint_x = sram_watchpoint_x & 0x7FFFFFFF;
  uint8_t diameter_adjust_lower =
      (int64_t)(sram_watchpoint_x - radius) < 0 && better_debug_tui
          ? -(int64_t)(sram_watchpoint_x - radius)
          : 0;
  uint8_t diameter_adjust_upper =
      sram_watchpoint_x + radius > (sram_size - 1) && better_debug_tui
          ? (sram_watchpoint_x + radius) - (sram_size - 1)
          : 0;

  if (ivt_max_idx != -1) {
    print_file_with_idcs(
        mem_type,
        max(0, sram_watchpoint_x - radius - diameter_adjust_upper +
                   (term_height % 2 == 1 && better_debug_tui ? 1 : 0)),
        min(sram_watchpoint_x + radius + diameter_adjust_lower, ivt_max_idx),
        true, false);
  }
  print_file_with_idcs(mem_type,
                       max(ivt_max_idx + 1, sram_watchpoint_x - radius -
                                                diameter_adjust_upper +
                                                (term_height % 2 == 1 && better_debug_tui ? 1 : 0)),
                       min(sram_watchpoint_x + radius + diameter_adjust_lower,
                           num_instrs_isrs + num_instrs_prgrm - 1),
                       false, true);
  print_file_with_idcs(
      mem_type,
      max(num_instrs_isrs + num_instrs_prgrm,
          sram_watchpoint_x - radius - diameter_adjust_upper +
              (term_height % 2 == 1 && better_debug_tui ? 1 : 0)),
      min(sram_watchpoint_x + radius + diameter_adjust_lower, sram_size - 1),
      false, false);
}

void print_uart_meta_data() {
  print_formatted_to_stdout_or_box("Current send data: %s\n", &uart_box,
                                   current_send_data ? current_send_data : "");
  print_formatted_to_stdout_or_box("All send data: %s\n", &uart_box,
                                   all_send_data ? all_send_data : "");
  print_formatted_to_stdout_or_box("Waiting time sending: ", &uart_box);
  print_formatted_to_stdout_or_box("%d\n", &uart_box, sending_waiting_time);
  print_formatted_to_stdout_or_box("Waiting time receiving: ", &uart_box);
  print_formatted_to_stdout_or_box("%d\n", &uart_box, receiving_waiting_time);
  if (receiving_waiting_time > 0) {
    print_formatted_to_stdout_or_box("Current input: %u\n", &uart_box,
                                     received_num_part);
  } else {
    print_formatted_to_stdout_or_box("Current input:\n", &uart_box);
  }
  print_formatted_to_stdout_or_box("Remaining input: ", &uart_box);
  if (read_metadata && input_idx < input_len) {
    for (uint8_t i = input_idx; i < input_len; i++) {
      if (i == input_idx && (int8_t)received_num_idx >= 0) {
        print_formatted_to_stdout_or_box("%d(", &uart_box, received_num);
        for (uint8_t j = received_num_idx; j != 0; j--) {
          uint8_t received_num_part =
              (received_num & (0xFF << (j * 8))) >> (j * 8);
          print_formatted_to_stdout_or_box("%u ", &uart_box, received_num_part);
        }
        uint8_t received_num_part = received_num & 0xFF;
        print_formatted_to_stdout_or_box("%u) ", &uart_box, received_num_part);
      } else {
        print_formatted_to_stdout_or_box("%d ", &uart_box, uart_input[i]);
      }
    }
  } else {
    if ((int8_t)received_num_idx >= 0) {
      print_formatted_to_stdout_or_box("%d(", &uart_box, received_num);
      for (uint8_t j = received_num_idx; j != 0; j--) {
        uint8_t received_num_part =
            (received_num & (0xFF << (j * 8))) >> (j * 8);
        print_formatted_to_stdout_or_box("%u ", &uart_box, received_num_part);
      }
      uint8_t received_num_part = received_num & 0xFF;
      print_formatted_to_stdout_or_box("%u)", &uart_box, received_num_part);
    }
  }
  print_formatted_to_stdout_or_box("\n", &uart_box);
}

void get_user_input(void) {
  uint8_t count;
  char buffer[26];
  while (true) {
    if (better_debug_tui) {
      int ch = getchar();
      if (ch == EOF) {
        continue;
      }
      buffer[0] = (char)ch;
      buffer[1] = '\0';
    } else {
      if (fgets(buffer, sizeof(buffer), stdin) == NULL) {
        fprintf(stderr, "Error: Reading input not successful\n");
        continue;
      }
      printf("\033[A\033[K");
      if (invalid_input) {
        printf("\033[A\033[K");
        invalid_input = false;
      }
      fflush(stdout);
    }

    char **split_input = split_string(buffer, &count);
    if (split_input == NULL) {
      continue;
    } else if (strcmp(split_input[0], "n") == 0 && count == 1) {
      return;
    } else if (strcmp(split_input[0], "c") == 0 && count == 1) {
      breakpoint_encountered = false;
      return;
    } else if (strcmp(split_input[0], "s") == 0 && count == 1) {
      if (machine_to_assembly(read_storage(read_array(regs, PC, false)))->op !=
          INT) {
        continue;
      }
      step_into_activated = true;
      return;
    } else if (strcmp(split_input[0], "a") == 0 && count == 3) {
      if (strcmp(split_input[1], "e") == 0) {
        char *eprom_watchpoint_tmp = eprom_watchpoint;
        eprom_watchpoint = split_input[2];
        if (!draw_tui()) {
          eprom_watchpoint = eprom_watchpoint_tmp;
        }
      } else if (strcmp(split_input[1], "sc") == 0) {
        char *sram_watchpoint_cs_tmp = sram_watchpoint_cs;
        sram_watchpoint_cs = split_input[2];
        if (!draw_tui()) {
          sram_watchpoint_cs = sram_watchpoint_cs_tmp;
        }
      } else if (strcmp(split_input[1], "sd") == 0) {
        char *sram_watchpoint_ds_tmp = sram_watchpoint_ds;
        sram_watchpoint_ds = split_input[2];
        if (!draw_tui()) {
          sram_watchpoint_ds = sram_watchpoint_ds_tmp;
        }
      } else if (strcmp(split_input[1], "ss") == 0) {
        char *sram_watchpoint_stack_tmp = sram_watchpoint_stack;
        sram_watchpoint_stack = split_input[2];
        if (!draw_tui()) {
          sram_watchpoint_stack = sram_watchpoint_stack_tmp;
        }
      } else {
        fprintf(stderr, "Error: Invalid command\n");
        invalid_input = true;
        continue;
      }
      continue;
    } else if (strcmp(split_input[0], "D") == 0) {
#ifdef __linux__
      __asm__("int3"); // ../.gdbinit
#endif
    } else if (strcmp(split_input[0], "q") == 0 && count == 1) {
      exit(EXIT_SUCCESS);
    } else {
      if (!better_debug_tui) {
        fprintf(stderr, "Error: Invalid command\n");
        invalid_input = true;
      }
    }
  }
}

void handle_heading(bool better_debug_tui, bool simple_debug_tui, Box *box,
                    char *format_str, char *watchpoint,
                    uint64_t watchpoint_int) {
  if (better_debug_tui) {
    if (simple_debug_tui) {
      box->title = malloc(strlen(format_str) + 1);
      strcpy(box->title, format_str);
    } else {
      uint8_t len_title =
          snprintf(NULL, 0, format_str, watchpoint, watchpoint_int) + 1;
      box->title = malloc(len_title);
      snprintf(box->title, len_title, format_str, watchpoint, watchpoint_int);
    }
  } else {
    if (simple_debug_tui) {
      printf("%s\n", create_heading('-', format_str, LINEWIDTH));
    } else {
      printf(format_str, watchpoint, watchpoint_int);
      printf("\n");
    }
  }
}

bool draw_tui(void) {
  uint64_t eprom_watchpoint_int = determine_watchpoint_value(eprom_watchpoint);
  uint64_t sram_watchpoint_cs_int =
      determine_watchpoint_value(sram_watchpoint_cs);
  uint64_t sram_watchpoint_ds_int =
      determine_watchpoint_value(sram_watchpoint_ds);
  uint64_t sram_watchpoint_stack_int =
      determine_watchpoint_value(sram_watchpoint_stack);
  if (eprom_watchpoint_int == UINT64_MAX ||
      sram_watchpoint_cs_int == UINT64_MAX ||
      sram_watchpoint_ds_int == UINT64_MAX ||
      sram_watchpoint_stack_int == UINT64_MAX) {
    return false;
  }

  if (better_debug_tui) {
    clear();
    for (int i = 0; i < box_length; i++) {
      reset_box_line(boxes[i]);
    }
  } else {
    clrscr();
  }

  handle_heading(better_debug_tui, true, &regs_box, "Registers", "", 0);
  print_array_with_idcs(REGS, NUM_REGISTERS, false);

  if (!better_debug_tui) {
    handle_heading(false, true, &eprom_box, "EPROM", "", 0);
  }
  handle_heading(better_debug_tui, false, &eprom_box, "EPROM (e): %s (%lu)",
                 eprom_watchpoint, eprom_watchpoint_int);
  print_eprom_watchpoint(eprom_watchpoint_int);

  handle_heading(better_debug_tui, true, &uart_box, "UART", "", 0);
  print_array_with_idcs(UART, NUM_UART_ADDRESSES, false);
  print_uart_meta_data();

  // the user shouldn't have to calculate the absolute address for the sram
  sram_watchpoint_cs_int =
      sram_watchpoint_cs_int + (isdigit(sram_watchpoint_cs[0]) ? (1 << 31) : 0);
  sram_watchpoint_ds_int =
      sram_watchpoint_ds_int + (isdigit(sram_watchpoint_ds[0]) ? (1 << 31) : 0);
  sram_watchpoint_stack_int =
      sram_watchpoint_stack_int +
      (isdigit(sram_watchpoint_stack[0]) ? (1 << 31) : 0);
  if (!better_debug_tui) {
    handle_heading(false, true, &regs_box, "SRAM", "", 0);
  }
  handle_heading(better_debug_tui, false, &sram_c_box,
                 "SRAM Codesegment (sc): %s (%lu)", sram_watchpoint_cs,
                 sram_watchpoint_cs_int);
  print_sram_watchpoint(sram_watchpoint_cs_int, SRAM_C);

  handle_heading(better_debug_tui, false, &sram_d_box,
                 "SRAM Datasegment (sd): %s (%lu)", sram_watchpoint_ds,
                 sram_watchpoint_ds_int);
  print_sram_watchpoint(sram_watchpoint_ds_int, SRAM_D);

  handle_heading(better_debug_tui, false, &sram_s_box,
                 "SRAM Stack (ss): %s (%lu)", sram_watchpoint_stack,
                 sram_watchpoint_stack_int);
  print_sram_watchpoint(sram_watchpoint_stack_int, SRAM_S);

  if (better_debug_tui) {
    if (extended_features) {
      mvprintw(term_height - 1, 0,
               "(n)ext instruction, (c)ontinue to breakpoint, (q)uit, (s)tep "
               "into isr, (a)ssign watchpoint reg or addr");
    } else {
      mvprintw(term_height - 1, 0,
               "(n)ext instruction, (c)ontinue to breakpoint, (q)uit, "
               "(a)ssign watchpoint reg or addr");
    }
  } else {
    printf("%s\n", create_heading('=', "Possible actions", LINEWIDTH));
    printf("(n)ext instruction, (c)ontinue to breakpoint, (q)uit\n");
    if (extended_features) {
      printf("(s)tep into isr, (a)ssign watchpoint reg or addr\n");
    } else {
      printf("(a)ssign watchpoint reg or addr\n");
    }
  }

  if (better_debug_tui) {
    for (int i = 0; i < box_length; i++) {
      draw_box(boxes[i]);
    }
    refresh(); // Refresh to display changes
  }

  return true;
}

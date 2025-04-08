#include "../include/error.h"
#include "../include/assemble.h"
#include "../include/debug.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/special_opts.h"
#include "../include/utils.h"
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ErrorContext error_context;

void display_error_message(const char *error_type, const char *error_message,
                           const char *to_insert,
                           ErrorContextType error_context_type) {
  uint32_t rel_addr;
  uint8_t memory_map_const;
  switch (error_context_type) {
  case Pntr:
    adjust_print(false, "%s:", NULL, error_context.filename);
    adjust_print(
        false, "%d: ", NULL,
        count_lines(error_context.code_current, error_context.code_begin));
    break;
  case Idx: {
    uint32_t addr = read_array(regs, PC, false);
    memory_map_const = addr >> 30;
    switch (memory_map_const) {
    case EPROM_CONST:
      rel_addr = addr;
      adjust_print(false, "%s:", NULL, eprom_prgrm_path);
      adjust_print(false, "i%d: ", NULL, addr + 1);
      break;
    case UART_CONST:
      rel_addr = addr & 0x3FFFFFFF;
      adjust_print(false,
                   "UART content of register %s interpreted as instruction: ",
                   NULL, rel_addr);
      break;
    default: // SRAM_CONST
      rel_addr = addr & 0x7FFFFFFF;
      if (rel_addr < num_instrs_isrs) {
        adjust_print(false, "%s:", NULL, isrs_prgrm_path);
        adjust_print(false, "i%d: ", NULL, rel_addr + 1);
      } else {
        adjust_print(false, "%s:", NULL, sram_prgrm_path);
        adjust_print(false, "i%d: ", NULL, rel_addr - num_instrs_isrs + 1);
      }
      break;
    }
  } break;
  default:
    fprintf(stderr, "Error: Invalid error context type\n");
    exit(EXIT_FAILURE);
  }

  adjust_print(false, "%s: ", NULL, error_type);
  adjust_print(true, NULL, "%s ", error_type);

  char *error_message_inserted;
  if (to_insert != NULL) {
    error_message_inserted =
        malloc(strlen(error_message) + strlen(to_insert) + 1);
    sprintf(error_message_inserted, error_message, to_insert);
  } else {
    error_message_inserted = (char *)error_message;
  }
  adjust_print(false, "%s\n", NULL, error_message_inserted);

  switch (error_context_type) {
  case Pntr:
    adjust_print(
        false, "%s\n", NULL,
        extract_line(error_context.code_current, error_context.code_begin));
    break;
  case Idx:
    switch (memory_map_const) {
    case EPROM_CONST:
      adjust_print(false, "%s\n", NULL,
                   assembly_to_str(machine_to_assembly(
                       read_array(eprom, rel_addr, false))));
      break;
    case UART_CONST:
      adjust_print(false, "%s\n", NULL,
                   assembly_to_str(
                       machine_to_assembly(read_array(uart, rel_addr, true))));
      break;
    default: // SRAM_CONST
      adjust_print(
          false, "%s\n", NULL,
          assembly_to_str(machine_to_assembly(read_file(sram, rel_addr))));
      break;
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid error context type\n");
    exit(EXIT_FAILURE);
  }
}

void check_opd(OperandType opd_expected, char *opd) {
  switch (opd_expected) {
  case REG:
    if (!isalpha(opd[0])) {
      display_error_message(
          "SyntaxError",
          "Invalid syntax for instruction, expected register, got \"%s\"", opd,
          Pntr);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    break;
  case IM:
    if (!(isdigit(opd[0]) || opd[0] == '-')) {
      display_error_message(
          "SyntaxError",
          "Invalid syntax for instruction, expected immediate, got \"%s\"", opd,
          Pntr);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    break;
  case EMPTY:
    if (!(strcmp(opd, "") == 0)) {
      display_error_message("SyntaxError",
                            "Invalid syntax for instruction, expected no more "
                            "operands, got \"%s\"",
                            opd, Pntr);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid operand type\n");
    exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
  }
}

void check_instr(uint8_t op, String_Instruction *str_instr) {
  if ((ADDI <= op && op <= ANDI) || (ADDM <= op && op <= ANDM)) {
    check_opd(REG, str_instr->opd1);
    check_opd(IM, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (ADDR <= op && op <= ANDR) {
    check_opd(REG, str_instr->opd1);
    check_opd(REG, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == LOAD || op == LOADI) {
    check_opd(REG, str_instr->opd1);
    check_opd(IM, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == LOADIN) {
    check_opd(REG, str_instr->opd1);
    check_opd(REG, str_instr->opd2);
    check_opd(IM, str_instr->opd3);
  } else if (op == STORE) {
    check_opd(REG, str_instr->opd1);
    check_opd(IM, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == STOREIN) {
    check_opd(REG, str_instr->opd1);
    check_opd(REG, str_instr->opd2);
    check_opd(IM, str_instr->opd3);
  } else if (op == MOVE) {
    check_opd(REG, str_instr->opd1);
    check_opd(REG, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if ((JUMPGT <= op && op <= JUMP) || op == INT) {
    check_opd(IM, str_instr->opd1);
    check_opd(EMPTY, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == NOP || op == RTI) {
    check_opd(EMPTY, str_instr->opd1);
    check_opd(EMPTY, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == IVTE) {
    check_opd(IM, str_instr->opd1);
    check_opd(EMPTY, str_instr->opd2);
    check_opd(EMPTY, str_instr->opd3);
  } else if (op == IVTEDP) {
    check_opd(IM, str_instr->opd1);
    check_opd(REG, str_instr->opd2);
    check_opd(IM, str_instr->opd3);
  } else {
    fprintf(stderr, "Error: Invalid opcode\n");
    exit(EXIT_FAILURE);
  }
}

void check_im(uint8_t op, uint64_t im, char *str) {
  if (op == STORE || op == LOAD || (ADDM <= op && op <= ANDM)) {
    if (0 <= im && im <= 4194303) {
      return;
    }
    display_error_message("SyntaxError",
                          "In this context Immediate is expected to be between "
                          "0 and 4194303, got \"%s\"",
                          str, Pntr);
    exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
  } else {
    if (-2097152 <= (int64_t)im && (int64_t)im <= 2097151) {
      return;
    }
    display_error_message("SyntaxError",
                          "In this context Immediate is expected to be between "
                          "-2097152 and 2097151, got \"%s\"",
                          str, Pntr);
    exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
  }
}

#include "../include/error.h"
#include "../include/parse_args.h"
#include "../include/parse_instrs.h"
#include "../include/utils.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: wrong name
const char *register_code_to_name[] = {"PC", "IN1", "IN2", "ACC",
                                       "SP", "BAF", "CS",  "DS"};

String_to_Mnemonic mnemonic_to_opcode[] = {
    {"ADDI", ADDI},     {"SUBI", SUBI},     {"MULTI", MULTI},
    {"DIVI", DIVI},     {"MODI", MODI},     {"OPLUSI", OPLUSI},
    {"ORI", ORI},       {"ANDI", ANDI},     {"ADD", ADDR},
    {"SUB", SUBR},      {"MULT", MULTR},    {"DIV", DIVR},
    {"MOD", MODR},      {"OPLUS", OPLUSR},  {"OR", ORR},
    {"AND", ANDR},      {"LOAD", LOAD},     {"LOADIN", LOADIN},
    {"LOADI", LOADI},   {"STORE", STORE},   {"STOREIN", STOREIN},
    {"MOVE", MOVE},     {"JUMP>", JUMPGT},  {"JUMP==", JUMPEQ},
    {"JUMP=", JUMPEQ},  {"JUMP>=", JUMPGE}, {"JUMP<", JUMPLT},
    {"JUMP!=", JUMPNE}, {"JUMP<>", JUMPNE}, {"JUMP<=", JUMPLE},
    {"JUMP", JUMP},     {"INT", INT},       {"RTI", RTI},
    {"NOP", NOP}};

String_to_Directive mnemonic_to_directive[] = {
    {"IVTE", IVTE},
};

uint8_t get_register_code(char *reg) {
  for (uint8_t i = 0;
       i < sizeof(register_code_to_name) / sizeof(register_code_to_name[0]);
       i++) {
    if (strcmp(reg, register_code_to_name[i]) == 0) {
      return i;
    }
  }

  display_error_message("SyntaxError", "Invalid register \"%s\"", reg, Pntr);
  exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
}

uint8_t get_mnemonic(char *mnemonic) {
  for (uint8_t i = 0;
       i < sizeof(mnemonic_to_opcode) / sizeof(mnemonic_to_opcode[0]); ++i) {
    if (strcmp(mnemonic, mnemonic_to_opcode[i].name) == 0) {
      return (uint8_t)mnemonic_to_opcode[i].value;
    }
  }
  for (uint8_t i = 0;
       i < sizeof(mnemonic_to_directive) / sizeof(mnemonic_to_directive[0]);
       ++i) {
    if (strcmp(mnemonic, mnemonic_to_directive[i].name) == 0) {
      return (uint8_t)mnemonic_to_directive[i].value;
    }
  }

  display_error_message("SyntaxError", "Invalid mnemonic \"%s\"", mnemonic,
                        Pntr);
  exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
}

uint32_t get_im(char *str, uint8_t op) {
  char *endptr;
  errno = 0;
  uint64_t tmp_val = strtol(str, &endptr, 10);

  // TODO: not sure this is correct
  if (errno == ERANGE) {
    fprintf(stderr, "Error: Immediate is way too large, it is not even between -9223372036854775808 and 9223372036854775807\n");
    exit(EXIT_FAILURE);
  }

  if (*endptr != '\0') {
    fprintf(stderr, "Further characters after number: %s\n", endptr);
    exit(EXIT_FAILURE);
  }
  check_im(op, tmp_val, str);
  return tmp_val & IMMEDIATE_MASK;
}

uint32_t assembly_to_machine(String_Instruction *str_instr) {
  uint32_t machine_instr = 0;
  uint8_t op = get_mnemonic(str_instr->op);

  if (ADDR <= op && op <= ANDR) {
    if (isdigit(str_instr->opd2[0])) {
      op += 8;
    }
  }

  // TODO: Add commandline option to only make this check if it's active
  check_instr(op, str_instr);

  uint32_t opd1, opd2, opd3;

  if (strcmp(str_instr->opd1, "") != 0) {
    if (isalpha(str_instr->opd1[0])) {
      opd1 = get_register_code(str_instr->opd1);
    } else {
      opd1 = get_im(str_instr->opd1, op);
    }
  } else {
    opd1 = 0;
  }

  if (strcmp(str_instr->opd2, "") != 0) {
    if (isalpha(str_instr->opd2[0])) {
      opd2 = get_register_code(str_instr->opd2);
    } else {
      opd2 = get_im(str_instr->opd2, op);
    }
  } else {
    opd2 = 0;
  }

  if (strcmp(str_instr->opd3, "") != 0) {
    if (isalpha(str_instr->opd3[0])) {
      opd3 = get_register_code(str_instr->opd3);
    } else {
      opd3 = get_im(str_instr->opd3, op);
    }
  } else {
    opd3 = 0;
  }

  if ((ADDI <= op && op <= ANDI) || (ADDM <= op && op <= ANDM)) {
    machine_instr = op << 25 | opd1 << 22 | opd2;
  } else if (ADDR <= op && op <= ANDR) {
    machine_instr = op << 25 | opd1 << 22 | opd2 << 19;
  } else if (op == LOAD || op == LOADI) {
    machine_instr = op << 25 | opd1 << 22 | opd2;
  } else if (op == LOADIN) {
    machine_instr = op << 25 | opd1 << 25 | opd2 << 22 | opd3;
  } else if (op == STORE) {
    machine_instr = op << 25 | opd1 << 25 | opd2;
  } else if (op == STOREIN) {
    machine_instr = op << 25 | opd1 << 22 | opd2 << 25 | opd3;
  } else if (op == MOVE) {
    machine_instr = op << 25 | opd1 << 25 | opd2 << 22;
  } else if ((JUMPGT <= op && op <= JUMP) || op == INT) {
    machine_instr = op << 25 | opd1;
  } else if (op == NOP || op == RTI) {
    machine_instr = op << 25;
  } else if (op == IVTE) {
    machine_instr = 0b10 << 30 | opd1;
  } else {
    fprintf(stderr, "Error: Invalid opcode\n");
    exit(EXIT_FAILURE);
  }

  return machine_instr;
}

Instruction *machine_to_assembly(uint32_t machine_instr) {
  Instruction *instr = malloc(sizeof(Instruction));
  memset(instr, 0, sizeof(Instruction));
  uint8_t mode = machine_instr >> 30;
  if (mode == COMPUTE_M) {
    uint8_t compute_mode = machine_instr >> 25;
    uint8_t d = (machine_instr >> 22) & REGISTER_MASK;
    uint8_t s = (machine_instr >> 19) & REGISTER_MASK;
    uint32_t i;
    if (ADDI <= compute_mode && compute_mode <= ANDI) {
      i = sign_extend_22_to_32(machine_instr & IMMEDIATE_MASK);
    } else if (ADDM <= compute_mode && compute_mode <= ANDM) {
      i = machine_instr & IMMEDIATE_MASK;
    } else if (ADDR <= compute_mode && compute_mode <= ANDR) {
    } // TODO: else case error

    instr->op = compute_mode;

    if ((ADDI <= compute_mode && compute_mode <= ANDI) ||
        (ADDM <= compute_mode && compute_mode <= ANDM)) {
      instr->opd1 = (uint32_t)d;
      instr->opd2 = (uint32_t)i;
    } else if (ADDR <= compute_mode && compute_mode <= ANDR) {
      instr->opd1 = (uint32_t)d;
      instr->opd2 = (uint32_t)s;
    } else {
      fprintf(stderr,
              "Error: A instruction with this opcode doesn't exist yet\n");
      exit(EXIT_FAILURE);
    }
  } else if (mode == LOAD_M || mode == STORE_M) {
    uint8_t load_store_mode = machine_instr >> 28;
    load_store_mode = load_store_mode << 3;
    uint8_t s = (machine_instr >> 25) & REGISTER_MASK;
    uint8_t d = (machine_instr >> 22) & REGISTER_MASK;
    uint32_t i;

    if (load_store_mode == LOAD || load_store_mode == STORE) {
      i = machine_instr & IMMEDIATE_MASK;
    } else if (load_store_mode == LOADIN || load_store_mode == STOREIN ||
               load_store_mode == LOADI) {
      i = sign_extend_22_to_32(machine_instr & IMMEDIATE_MASK);
    }
    // TODO: LOADI in folien ändern, dass das i signed ist

    instr->op = load_store_mode;

    switch (load_store_mode) {
    case LOAD:
      instr->opd1 = d;
      instr->opd2 = i;
      break;
    case LOADIN:
      instr->opd1 = s;
      instr->opd2 = d;
      instr->opd3 = i;
      break;
    case LOADI:
      instr->opd1 = d;
      instr->opd2 = i;
      break;
    case STORE:
      instr->opd1 = s;
      instr->opd2 = i;
      break;
    case STOREIN:
      instr->opd1 = d;
      instr->opd2 = s;
      instr->opd3 = i;
      break;
    case MOVE:
      instr->opd1 = s;
      instr->opd2 = d;
      break;
    default:
      fprintf(stderr,
              "Error: A instruction with this opcode doesn't exist yet\n");
      exit(EXIT_FAILURE);
    }
  } else { // mode == JUMP_M
    uint8_t jump_mode = machine_instr >> 25;
    uint32_t i = 0;
    if (jump_mode == INT) {
      i = machine_instr & IMMEDIATE_MASK;
    } else if (jump_mode == RTI || jump_mode == NOP) {
    } else {
      i = sign_extend_22_to_32(machine_instr & IMMEDIATE_MASK);
    }

    instr->op = jump_mode;

    if ((JUMPGT <= jump_mode && jump_mode <= JUMP) || jump_mode == INT) {
      instr->opd1 = i;
    } else if (jump_mode == RTI || jump_mode == NOP) {
    } else {
      fprintf(stderr,
              "Error: A instruction with this opcode doesn't exist yet\n");
      exit(EXIT_FAILURE);
    }
  }
  return instr;
}

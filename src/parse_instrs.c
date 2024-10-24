#include "../include/parse_instrs.h"
#include "../include/interpr.h"
#include "../include/reti.h"
#include "../include/error.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Irgendwie durch untere Funktionen dafür sorgen, dass immer nur ein
// TODO: nen check machen, ob Zahl nicht zu lang, später in ner anderen

String_Instruction *parse_instr(const char **original_prgrm_pntr) {
  const char *prgrm_pntr = *original_prgrm_pntr;
  String_Instruction *str_instr = malloc(sizeof(String_Instruction));
  memset(str_instr, 0, sizeof(String_Instruction));
  uint8_t token_cnt = 0;
  uint8_t rel_idx;

  while (*prgrm_pntr == ' ') {
    prgrm_pntr++;
  }

  while (true) {
    rel_idx = 0;
    while (true) {
      if (*prgrm_pntr == ' ') {
        switch (token_cnt) {
        case 0:
          (str_instr->op)[rel_idx] = '\0';
          break;
        case 1:
          (str_instr->opd1)[rel_idx] = '\0';
          break;
        case 2:
          (str_instr->opd2)[rel_idx] = '\0';
          break;
        case 3:
          (str_instr->opd3)[rel_idx] = '\0';
          break;
        }
        while (*prgrm_pntr == ' ') {
          prgrm_pntr++;
        }
        break;
      } else if (*prgrm_pntr == ';' || *prgrm_pntr == '\n') {
        prgrm_pntr++;
        *original_prgrm_pntr = prgrm_pntr;
        return str_instr;
      } else if (*prgrm_pntr == '#') {
        while (*prgrm_pntr != '\n' && *prgrm_pntr != '\0') {
          prgrm_pntr++;
        }
        continue;
      } else if (*prgrm_pntr == '\0') {
        *original_prgrm_pntr = prgrm_pntr;
        return str_instr;
      }
      switch (token_cnt) {
      case 0:
        (str_instr->op)[rel_idx] = *prgrm_pntr;
        break;
      case 1:
        (str_instr->opd1)[rel_idx] = *prgrm_pntr;
        break;
      case 2:
        (str_instr->opd2)[rel_idx] = *prgrm_pntr;
        break;
      case 3:
        (str_instr->opd3)[rel_idx] = *prgrm_pntr;
        break;
      default:
        fprintf(stderr, "Error: Too many operands\n");
        exit(EXIT_FAILURE);
      }
      rel_idx++;
      prgrm_pntr++;
    }
    token_cnt++;
  }
}

void parse_and_load_program(char *prgrm, Program_Type prgrm_type) {
  const char *prgrm_pntr = prgrm;
  uint32_t i;
  if (prgrm_type == SRAM_PRGRM) {
    i = num_instrs_isrs;
  } else {
    i = 0;
  }

  error_context.code_begin = prgrm_pntr;
  while (*prgrm_pntr != '\0') {
    error_context.code_current = prgrm_pntr;
    String_Instruction *str_instr = parse_instr(&prgrm_pntr);
    if (isalpha(*str_instr->op)) {
      // the if solves the problem of empty lines or empty space between ';'
      uint32_t machine_instr = assembly_to_machine(str_instr);
      switch (prgrm_type) {
      case SRAM_PRGRM:
        write_file(sram, i++, machine_instr);
        break;
      case ISR_PRGRMS:
        if (strcmp(str_instr->op, "IVTE") == 0) {
          ivt_max_idx = i;
        }
        write_file(sram, i++, machine_instr);
        break;
      case EPROM_START_PRGRM:
        uint32_t *temp =
            realloc(eprom, sizeof(uint32_t) * i + sizeof(uint32_t));
        if (temp == NULL) {
          fprintf(stderr, "Realloc failed\n");
          free(eprom);
          exit(EXIT_FAILURE);
        }
        eprom = temp;
        write_array(eprom, i++, machine_instr, false);
        break;
      default:
        fprintf(stderr, "Error: Invalid memory type\n");
      }
    }
  }
  switch (prgrm_type) {
  case SRAM_PRGRM:
    num_instrs_prgrm = i - num_instrs_isrs;
    break;
  case ISR_PRGRMS:
    num_instrs_isrs = i;
    break;
  case EPROM_START_PRGRM:
    num_instrs_start_prgrm = i;
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
  }
  free(prgrm);
}

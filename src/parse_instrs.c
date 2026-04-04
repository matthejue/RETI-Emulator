#include "../include/parse_instrs.h"
#include "../include/error.h"
#include "../include/interpr.h"
#include "../include/reti.h"
#include "../include/parse_args.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// TODO: Irgendwie durch untere Funktionen dafür sorgen, dass immer nur ein
// TODO: nen check machen, ob Zahl nicht zu lang, später in ner anderen

Source_Comment *source_comments = NULL;
uint32_t num_source_comments = 0;

static char *copy_trimmed_comment_text(const char *start, size_t len) {
  while (len > 0 && isspace((unsigned char)*start)) {
    start++;
    len--;
  }
  while (len > 0 && isspace((unsigned char)start[len - 1])) {
    len--;
  }

  char *text = malloc(len + 1);
  strncpy(text, start, len);
  text[len] = '\0';
  return text;
}

static bool segment_contains_instruction(const char *start, size_t len) {
  while (len > 0 && isspace((unsigned char)*start)) {
    start++;
    len--;
  }
  return len > 0 && isalpha((unsigned char)*start);
}

static void append_source_comment(Comment_Target target, uint32_t anchor_idx,
                                  bool display_before_instr,
                                  const char *text_start, size_t text_len) {
  char *text = copy_trimmed_comment_text(text_start, text_len);
  if (text[0] == '\0') {
    free(text);
    return;
  }

  Source_Comment *tmp =
      realloc(source_comments, sizeof(Source_Comment) * (num_source_comments + 1));
  if (tmp == NULL) {
    fprintf(stderr, "Realloc failed\n");
    free(text);
    exit(EXIT_FAILURE);
  }

  source_comments = tmp;
  source_comments[num_source_comments++] = (Source_Comment){
      .target = target,
      .anchor_idx = anchor_idx,
      .display_before_instr = display_before_instr,
      .text = text,
  };
}

void collect_program_comments(const char *prgrm, Program_Type prgrm_type) {
  if (!collect_comments) {
    return;
  }

  Comment_Target target =
      prgrm_type == EPROM_START_PRGRM ? COMMENT_TARGET_EPROM : COMMENT_TARGET_SRAM;
  uint32_t next_instr_idx = prgrm_type == SRAM_PRGRM ? num_instrs_isrs : 0;
  bool saw_instr_in_program = false;
  const char *line = prgrm;

  while (*line != '\0') {
    const char *line_end = line;
    while (*line_end != '\0' && *line_end != '\n' && *line_end != '\r') {
      line_end++;
    }

    const char *comment_start = NULL;
    for (const char *ptr = line; ptr < line_end; ptr++) {
      if (*ptr == '#') {
        comment_start = ptr;
        break;
      }
    }

    const char *code_end = comment_start != NULL ? comment_start : line_end;
    uint32_t instrs_on_line = 0;
    const char *segment_start = line;
    for (const char *ptr = line; ptr <= code_end; ptr++) {
      if (ptr == code_end || *ptr == ';') {
        if (segment_contains_instruction(segment_start, ptr - segment_start)) {
          instrs_on_line++;
        }
        segment_start = ptr + 1;
      }
    }

    if (comment_start != NULL) {
      if (instrs_on_line > 0) {
        append_source_comment(target, next_instr_idx + instrs_on_line - 1, false,
                              comment_start + 1, line_end - comment_start - 1);
      } else if (!saw_instr_in_program) {
        append_source_comment(target, next_instr_idx, true, comment_start + 1,
                              line_end - comment_start - 1);
      } else {
        append_source_comment(target, next_instr_idx - 1, false,
                              comment_start + 1, line_end - comment_start - 1);
      }
    }

    next_instr_idx += instrs_on_line;
    saw_instr_in_program = saw_instr_in_program || instrs_on_line > 0;

    if (*line_end == '\r' && *(line_end + 1) == '\n') {
      line = line_end + 2;
    } else if (*line_end == '\n' || *line_end == '\r') {
      line = line_end + 1;
    } else {
      line = line_end;
    }
  }
}

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
      if (*prgrm_pntr == ' ' || *prgrm_pntr == '\t') {
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
        while (*prgrm_pntr == ' ' || *prgrm_pntr == '\t') {
          prgrm_pntr++;
        }
        break;
      } else if (*prgrm_pntr == ';' || *prgrm_pntr == '\n' ||
                 *prgrm_pntr == '\r') {
        prgrm_pntr++;
        *original_prgrm_pntr = prgrm_pntr;
        return str_instr;
      } else if (*prgrm_pntr == '#') {
        while (*prgrm_pntr != '\n' && *prgrm_pntr != '\0' &&
               *prgrm_pntr != '\r') {
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
      default: {
        char *opds =
            malloc(strlen(str_instr->op) + strlen(str_instr->opd1) +
                   strlen(str_instr->opd2) + strlen(str_instr->opd3) + 4);
        strcpy(opds, str_instr->op);
        strcat(opds, " ");
        strcat(opds, str_instr->opd1);
        strcat(opds, " ");
        strcat(opds, str_instr->opd2);
        strcat(opds, " ");
        strcat(opds, str_instr->opd3);
        display_error_message("SyntaxError", "For sure too many oparnds after \"%s\"", opds, Pntr);
        exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
      }
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

  collect_program_comments(prgrm, prgrm_type);

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
      case EPROM_START_PRGRM: {
        uint32_t *temp;
        temp = realloc(eprom, sizeof(uint32_t) * i + sizeof(uint32_t));
        if (temp == NULL) {
          fprintf(stderr, "Realloc failed\n");
          free(eprom);
          exit(EXIT_FAILURE);
        }
        eprom = temp;
        write_array(eprom, i++, machine_instr, false);
      } break;
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

void free_program_comments(void) {
  for (uint32_t i = 0; i < num_source_comments; i++) {
    free(source_comments[i].text);
  }
  free(source_comments);
  source_comments = NULL;
  num_source_comments = 0;
}

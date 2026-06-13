#include "../../include/parse/parse_instrs.h"
#include "../../include/error.h"
#include "../../include/interpr.h"
#include "../../include/interrupt_controller.h"
#include "../../include/reti.h"
#include "../../include/parse/parse_args.h"
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
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
  return len > 0 &&
         (isalpha((unsigned char)*start) || isdigit((unsigned char)*start) ||
          *start == '-' || *start == '\'');
}

static bool is_segment_end(char c) {
  return c == '\0' || c == ';' || c == '\n' || c == '\r' || c == '#';
}

static void skip_to_next_segment(const char **prgrm_pntr) {
  while (!is_segment_end(**prgrm_pntr)) {
    (*prgrm_pntr)++;
  }
  if (**prgrm_pntr == '#') {
    while (**prgrm_pntr != '\0' && **prgrm_pntr != '\n' &&
           **prgrm_pntr != '\r') {
      (*prgrm_pntr)++;
    }
  }
  if (**prgrm_pntr != '\0') {
    (*prgrm_pntr)++;
  }
}

static bool parse_numeric_memory_word(const char **prgrm_pntr,
                                      uint32_t *value) {
  const char *start = *prgrm_pntr;
  while (*start == ' ' || *start == '\t') {
    start++;
  }
  if (!isdigit((unsigned char)*start) && *start != '-') {
    return false;
  }

  char *endptr;
  errno = 0;
  if (*start == '-') {
    long long signed_value = strtoll(start, &endptr, 10);
    if (errno == ERANGE || signed_value < INT32_MIN ||
        signed_value > INT32_MAX) {
      char *word = copy_trimmed_comment_text(start, endptr - start);
      display_error_message(
          "SyntaxError",
          "Signed memory word \"%s\" is not a 32-bit signed number", word,
          Pntr);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }

    const char *ptr = endptr;
    while (*ptr == ' ' || *ptr == '\t') {
      ptr++;
    }
    if (!is_segment_end(*ptr)) {
      return false;
    }

    *value = (uint32_t)(int32_t)signed_value;
    *prgrm_pntr = ptr;
    skip_to_next_segment(prgrm_pntr);
    return true;
  }

  unsigned long long unsigned_value = strtoull(start, &endptr, 10);
  if (errno == ERANGE || unsigned_value > UINT32_MAX) {
    char *word = copy_trimmed_comment_text(start, endptr - start);
    display_error_message(
        "SyntaxError",
        "Unsigned memory word \"%s\" is not a 32-bit unsigned number", word,
        Pntr);
    exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  const char *ptr = endptr;
  while (*ptr == ' ' || *ptr == '\t') {
    ptr++;
  }
  if (!is_segment_end(*ptr)) {
    return false;
  }

  *value = (uint32_t)unsigned_value;
  *prgrm_pntr = ptr;
  skip_to_next_segment(prgrm_pntr);
  return true;
}

static bool parse_ascii_memory_word(const char **prgrm_pntr, uint32_t *value) {
  const char *start = *prgrm_pntr;
  while (*start == ' ' || *start == '\t') {
    start++;
  }
  if (*start != '\'' || *(start + 2) != '\'') {
    return false;
  }

  const char *ptr = start + 3;
  while (*ptr == ' ' || *ptr == '\t') {
    ptr++;
  }
  if (!is_segment_end(*ptr)) {
    return false;
  }

  *value = (uint8_t)*(start + 1);
  *prgrm_pntr = ptr;
  skip_to_next_segment(prgrm_pntr);
  return true;
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

void collect_program_comments_range(const char *prgrm, Program_Type prgrm_type,
                                    uint32_t start_entry, uint32_t end_entry,
                                    uint32_t anchor_base) {
  if (!collect_comments) {
    return;
  }

  Comment_Target target =
      prgrm_type == EPROM_START_PRGRM ? COMMENT_TARGET_EPROM : COMMENT_TARGET_SRAM;
  uint32_t next_entry_idx = 0;
  bool saw_entry_in_range = false;
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
      uint32_t anchor_entry = 0;
      bool display_before_instr = false;
      if (instrs_on_line > 0) {
        anchor_entry = next_entry_idx + instrs_on_line - 1;
      } else if (!saw_entry_in_range && next_entry_idx == start_entry) {
        anchor_entry = start_entry;
        display_before_instr = true;
      } else if (next_entry_idx > 0) {
        anchor_entry = next_entry_idx - 1;
      }

      if (anchor_entry >= start_entry &&
          (end_entry == UINT32_MAX || anchor_entry < end_entry)) {
        append_source_comment(target, anchor_base + anchor_entry - start_entry,
                              display_before_instr, comment_start + 1,
                              line_end - comment_start - 1);
      }
    }

    if (instrs_on_line > 0 && next_entry_idx + instrs_on_line > start_entry &&
        (end_entry == UINT32_MAX || next_entry_idx < end_entry)) {
      saw_entry_in_range = true;
    }
    next_entry_idx += instrs_on_line;

    if (*line_end == '\r' && *(line_end + 1) == '\n') {
      line = line_end + 2;
    } else if (*line_end == '\n' || *line_end == '\r') {
      line = line_end + 1;
    } else {
      line = line_end;
    }
  }
}

void collect_program_comments(const char *prgrm, Program_Type prgrm_type) {
  uint32_t anchor_base = prgrm_type == SRAM_PRGRM ? num_instrs_isrs : 0;
  collect_program_comments_range(prgrm, prgrm_type, 0, UINT32_MAX, anchor_base);
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

static void write_machine_word(Program_Type prgrm_type, uint32_t idx,
                               uint32_t machine_word) {
  if (prgrm_type != EPROM_START_PRGRM) {
    write_file(sram, idx, machine_word);
    return;
  }

  uint32_t *temp;
  temp = realloc(eprom, sizeof(uint32_t) * idx + sizeof(uint32_t));
  if (temp == NULL) {
    fprintf(stderr, "Realloc failed\n");
    free(eprom);
    exit(EXIT_FAILURE);
  }
  eprom = temp;
  write_array(eprom, idx, machine_word, false);
}

static void count_isr_vector_entry(void) {
  if (isr_num >= INVALID_ISR_NUM) {
    fprintf(stderr,
            "Error: There can't be more than %d interrupt service routines\n",
            INVALID_ISR_NUM);
    exit(EXIT_FAILURE);
  }
  isr_num++;
}

void parse_and_load_program_range(char *prgrm, Program_Type prgrm_type,
                                  uint32_t start_entry, uint32_t end_entry) {
  const char *prgrm_pntr = prgrm;
  uint32_t i = 0;
  if (prgrm_type == SRAM_PRGRM) {
    i = num_instrs_isrs;
  } else if (prgrm_type == SRAM_DATA) {
    i = num_instrs_isrs + num_instrs_prgrm;
  }
  uint32_t parsed_entries = 0;
  uint32_t loaded_entries = 0;
  bool in_isr_vector_table = prgrm_type == ISR_PRGRMS && start_entry == 0;

  error_context.code_begin = prgrm_pntr;
  while (*prgrm_pntr != '\0') {
    error_context.code_current = prgrm_pntr;
    bool should_load =
        parsed_entries >= start_entry &&
        (end_entry == UINT32_MAX || parsed_entries < end_entry);
    uint32_t memory_word;
    if (parse_numeric_memory_word(&prgrm_pntr, &memory_word) ||
        parse_ascii_memory_word(&prgrm_pntr, &memory_word)) {
      if (should_load) {
        if (in_isr_vector_table) {
          count_isr_vector_entry();
        }
        write_machine_word(prgrm_type, i++, memory_word);
        loaded_entries++;
      }
      parsed_entries++;
      continue;
    }

    String_Instruction *str_instr = parse_instr(&prgrm_pntr);
    if (isalpha(*str_instr->op)) {
      in_isr_vector_table = false;
      // the if solves the problem of empty lines or empty space between ';'
      if (should_load) {
        uint32_t machine_instr = assembly_to_machine(str_instr);
        write_machine_word(prgrm_type, i++, machine_instr);
        loaded_entries++;
      }
      parsed_entries++;
    }
  }
  if (prgrm_type == SRAM_PRGRM) {
    num_instrs_prgrm = loaded_entries;
  } else if (prgrm_type == ISR_PRGRMS) {
    num_instrs_isrs = loaded_entries;
  } else if (prgrm_type == EPROM_START_PRGRM) {
    num_instrs_start_prgrm = loaded_entries;
  }
  free(prgrm);
}

void parse_and_load_program(char *prgrm, Program_Type prgrm_type) {
  collect_program_comments(prgrm, prgrm_type);
  parse_and_load_program_range(prgrm, prgrm_type, 0, UINT32_MAX);
}

void free_program_comments(void) {
  for (uint32_t i = 0; i < num_source_comments; i++) {
    free(source_comments[i].text);
  }
  free(source_comments);
  source_comments = NULL;
  num_source_comments = 0;
}

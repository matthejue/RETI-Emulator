#include "../include/error.h"
#include "../include/interpr.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/parse/parse_args.h"
#include "../include/parse/parse_instrs.h"
#include "../include/parse/parse_sections.h"
#include "../include/reti.h"
#include "../include/special_opts.h"
#include "../include/tui.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include "../include/core_debug.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void load_interrupt_setup(void) {
  if (strcmp(interrupt_controller_config_path, "") != 0) {
    load_interrupt_controller_config(interrupt_controller_config_path);
  }
  init_custom_interrupt_action_isr();
}

static Program_Sections empty_program_sections(void) {
  return (Program_Sections){.exists = false,
                            .codesegment_start = 0,
                            .datasegment_start = 0,
                            .stack_start = STACK_START_AUTO,
                            .has_stack_start = false};
}

static Program_Sections load_sram_program(Program_Sections *forced_sections) {
  Program_Sections sections = empty_program_sections();
  bool has_explicit_isrs = strcmp(isrs_prgrm_path, "") != 0;
  char *sram_prgrm_content = NULL;

  if (has_sram_prgrm) {
    if (forced_sections != NULL) {
      sections = *forced_sections;
    } else {
      sections = parse_sections_for_reti_path(sram_prgrm_path);
    }
    sram_prgrm_content = get_prgrm_content(sram_prgrm_path);
  }

  if (has_explicit_isrs) {
    error_context.filename = isrs_prgrm_path;
    parse_and_load_program(get_prgrm_content(isrs_prgrm_path), ISR_PRGRMS);
  } else if (sections.exists) {
    uint32_t code_start_idx = sections.codesegment_start;
    error_context.filename = sram_prgrm_path;
    parse_and_load_program_range(allocate_and_copy_string(sram_prgrm_content),
                                 ISR_PRGRMS, 0, code_start_idx);
  }

  load_interrupt_setup();

  if (!has_sram_prgrm) {
    return sections;
  }

  error_context.filename = sram_prgrm_path;
  if (sections.exists) {
    uint32_t code_start_idx = sections.codesegment_start;
    uint32_t data_start_idx = sections.datasegment_start;
    collect_program_comments_range(sram_prgrm_content, SRAM_PRGRM,
                                   code_start_idx, data_start_idx,
                                   num_instrs_isrs);
    parse_and_load_program_range(allocate_and_copy_string(sram_prgrm_content),
                                 SRAM_PRGRM, code_start_idx, data_start_idx);
    parse_and_load_program_range(sram_prgrm_content, SRAM_DATA, data_start_idx,
                                 UINT32_MAX);
  } else {
    parse_and_load_program(sram_prgrm_content, SRAM_PRGRM);
  }
  return sections;
}

static char *binary_output_path_for_reti_path(const char *reti_path) {
  char *bin_path = malloc(strlen(reti_path) + strlen(".bin") + 1);
  if (bin_path == NULL) {
    fprintf(stderr, "Malloc failed\n");
    exit(EXIT_FAILURE);
  }

  strcpy(bin_path, reti_path);
  char *last_slash = strrchr(bin_path, '/');
  char *ext = strrchr(last_slash == NULL ? bin_path : last_slash + 1, '.');
  if (ext != NULL) {
    *ext = '\0';
  }
  strcat(bin_path, ".bin");
  return bin_path;
}

static void assemble_sram_program_to_binary(void) {
  Program_Sections sections = parse_required_section_for_reti_path(sram_prgrm_path);
  load_sram_program(&sections);

  char *bin_path = binary_output_path_for_reti_path(sram_prgrm_path);
  FILE *bin_file = fopen(bin_path, "w+b");
  if (bin_file == NULL) {
    fprintf(stderr, "Error: Couldn't open binary output file %s\n", bin_path);
    free(bin_path);
    exit(EXIT_FAILURE);
  }

  uint32_t num_words = num_instrs_isrs + num_instrs_prgrm + num_instrs_data;
  write_file(bin_file, 0, sections.codesegment_start);
  write_file(bin_file, 1, sections.datasegment_start);
  write_file(bin_file, 2, sections.stack_start);
  for (uint32_t i = 0; i < num_words; i++) {
    write_file(bin_file, i + 3, read_file(sram, i));
  }
  fclose(bin_file);

  printf("Wrote %u section words and %u SRAM words to %s\n", 3, num_words,
         bin_path);
  free(bin_path);
}

int main(int argc, char *argv[]) {
  gargv = argv;

  parse_args(argc, argv);
  if (verbose) {
    print_args();
  }
  if (test_mode) {
    create_out_and_err_file();
  }
  if (read_metadata) {
    uart_input = extract_comment_metadata(sram_prgrm_path, &input_len);
  }

  init_reti();
  if (debug_mode && !assemble_mode) {
    init_tui();
  }

  if (assemble_mode) {
    assemble_sram_program_to_binary();
    finalize();
    return 0;
  }

  Program_Sections sections = load_sram_program(NULL);

  if (strcmp(eprom_prgrm_path, "") != 0) {
    if (!has_sram_prgrm) {
      Program_Sections eprom_sram_sections =
          parse_sections_for_reti_path(eprom_prgrm_path);
      set_eprom_only_sram_debug_sections(eprom_sram_sections.exists,
                                         eprom_sram_sections.codesegment_start,
                                         eprom_sram_sections.datasegment_start);
    }
    error_context.filename = eprom_prgrm_path;
    parse_and_load_program(get_prgrm_content(eprom_prgrm_path),
                           EPROM_START_PRGRM);
  } else {
    load_adjusted_eprom_prgrm(sections.stack_start);
  }

  interpr_prgrm();

  if (debug_mode && keep_tui_alive_after_halt) {
    wait_for_tui_quit();
  }

  finalize();

  return 0;
}

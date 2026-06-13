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
#include <string.h>

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
  if (debug_mode) {
    init_tui();
  }

  Program_Sections sections = parse_sections_for_reti_path(sram_prgrm_path);
  bool has_explicit_isrs = strcmp(isrs_prgrm_path, "") != 0;
  char *sram_prgrm_content = get_prgrm_content(sram_prgrm_path);

  if (has_explicit_isrs) {
    error_context.filename = isrs_prgrm_path;
    parse_and_load_program(get_prgrm_content(isrs_prgrm_path), ISR_PRGRMS);
  } else if (sections.exists) {
    uint32_t code_start_idx = sections.codesegment_start;
    error_context.filename = sram_prgrm_path;
    parse_and_load_program_range(allocate_and_copy_string(sram_prgrm_content),
                                 ISR_PRGRMS, 0, code_start_idx);
  }

  if (strcmp(interrupt_controller_config_path, "") != 0) {
    load_interrupt_controller_config(interrupt_controller_config_path);
  }
  init_keypress_interrupt_action_isr();

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

  if (strcmp(eprom_prgrm_path, "") != 0) {
    error_context.filename = eprom_prgrm_path;
    parse_and_load_program(get_prgrm_content(eprom_prgrm_path),
                           EPROM_START_PRGRM);
  } else {
    load_adjusted_eprom_prgrm();
  }

  interpr_prgrm();

  if (debug_mode && keep_tui_alive_after_halt) {
    wait_for_tui_quit();
  }

  finalize();

  return 0;
}

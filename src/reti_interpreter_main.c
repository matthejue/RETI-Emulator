#include "../include/error.h"
#include "../include/interpr.h"
#include "../include/parse_args.h"
#include "../include/parse_instrs.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include "../include/special_opts.h"
#include "../include/uart.h"
#include <string.h>

int main(int argc, char *argv[]) {
  parse_args(argc, argv);
  if (verbose) {
    print_args();
  }
  if (read_metadata) {
    uart_input = extract_comment_metadata(sram_prgrm_path, &input_len);
  }
  if (test_mode) {
    create_out_and_err_file();
  }

  init_reti();

  if (strcmp(isrs_prgrm_path, "") != 0) {
    error_context.filename = isrs_prgrm_path;
    parse_and_load_program(get_prgrm_content(isrs_prgrm_path), ISR_PRGRMS);
  }

  error_context.filename = sram_prgrm_path;
  parse_and_load_program(get_prgrm_content(sram_prgrm_path), SRAM_PRGRM);

  if (strcmp(eprom_prgrm_path, "") != 0) {
    error_context.filename = eprom_prgrm_path;
    parse_and_load_program(get_prgrm_content(eprom_prgrm_path),
                           EPROM_START_PRGRM);
  } else {
    load_adjusted_eprom_prgrm();
  }

  interpr_prgrm();

  fin_reti();
  if (test_mode) {
    close_out_and_err_file();
  }
  return 0;
}

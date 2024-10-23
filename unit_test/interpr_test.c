#include "../include/assert.h"
#include "../include/daemon.h"
#include "../include/interpr.h"
#include "../include/parse_args.h"
#include "../include/parse_instrs.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <stdlib.h>
#include <string.h>

void test_interpr_prgrm() {
  const char *test_input = "LOADI ACC 1;"
                           "STORE ACC 5;"
                           "ADD ACC 5;"
                           "SUBI ACC 3;"
                           "JUMP 0";

  size_t input_size = strlen(test_input);

  FILE *input_stream = fmemopen((void *)test_input, input_size, "r");
  if (input_stream == NULL) {
    fprintf(stderr, "Error: fmemopen failed");
    exit(EXIT_FAILURE);
  }

  FILE *original_stdin = stdin;
  stdin = input_stream;

  parse_args(4, (char *[]){"", "-f", "/tmp", "-"});
  init_reti();
  load_adjusted_eprom_prgrm();
  parse_and_load_program(get_prgrm_content(sram_prgrm_path), SRAM_PRGRM);
  interpr_prgrm();

  fclose(input_stream);
  stdin = original_stdin;

  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 0))),
                "LOADI ACC 1") == 0);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 1))),
                "STORE ACC 5") == 0);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 2))),
                "ADD ACC 5") == 0);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 3))),
                "SUBI ACC 3") == 0);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 4))),
                "JUMP 0") == 0);
  assert(strcmp(mem_value_to_str(read_file(sram, 5), false),
                "1") == 0);
}

int main() {
  test_interpr_prgrm();

  return 0;
}

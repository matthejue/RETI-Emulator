#include "../include/assemble.h"
#include "../include/parse/parse_args.h"
#include <assert.h>

int main(void) {
  char *argv[] = {"reti_emulator", "-n", "4", "--dma", "program.reti"};
  parse_args(5, argv);
  assert(has_sram_prgrm);
  assert(isr_num_override == 4);
  assert(dma_enabled);

  isr_num = 0;
  apply_isr_num_override();
  assert(isr_num == 4);
  return 0;
}

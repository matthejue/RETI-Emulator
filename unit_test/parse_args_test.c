#include "../include/parse/parse_args.h"
#include <assert.h>

int main(void) {
  char *argv[] = {"reti_emulator", "program.reti"};
  parse_args(2, argv);
  assert(has_sram_prgrm);
  return 0;
}

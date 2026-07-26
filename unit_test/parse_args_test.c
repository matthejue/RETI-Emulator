#include "../include/parse/parse_args.h"
#include <assert.h>

int main(void) {
  char *argv[] = {"reti_emulator", "-U", "program.reti"};
  parse_args(3, argv);
  assert(uart_mode);
  assert(has_sram_prgrm);
  return 0;
}

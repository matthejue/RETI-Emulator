#include "../include/tui.h"
#include <assert.h>

static void test_wide_terminal_layout(void) {
  calculate_tui_layout(83, 377);

  assert(term_height == 83);
  assert(term_width == 377);

  assert(regs_box.x == 0);
  assert(regs_box.y == 0);
  assert(regs_box.width == 94);
  assert(regs_box.height == 10);

  assert(eprom_box.x == 0);
  assert(eprom_box.y == 10);
  assert(eprom_box.width == 94);
  assert(eprom_box.height == 36);

  assert(uart_box.x == 0);
  assert(uart_box.y == 46);
  assert(uart_box.width == 94);
  assert(uart_box.height == 36);

  assert(sram_c_box.x == 94);
  assert(sram_c_box.width == 94);
  assert(sram_d_box.x == 188);
  assert(sram_d_box.width == 94);
  assert(sram_s_box.x == 282);
  assert(sram_s_box.width == 95);

  assert(sram_s_box.x + sram_s_box.width == term_width);
  assert(info_box.y == 82);
  assert(info_box.width == term_width);
}

static void test_large_terminal_dimensions_do_not_wrap(void) {
  calculate_tui_layout(300, 1200);

  assert(regs_box.width == 300);
  assert(eprom_box.height == 144);
  assert(uart_box.y == 154);
  assert(uart_box.height == 145);
  assert(sram_c_box.x == 300);
  assert(sram_d_box.x == 600);
  assert(sram_s_box.x == 900);
  assert(sram_s_box.width == 300);
  assert(sram_s_box.height == 299);
  assert(info_box.y == 299);
}

int main(void) {
  test_wide_terminal_layout();
  test_large_terminal_dimensions_do_not_wrap();
  return 0;
}

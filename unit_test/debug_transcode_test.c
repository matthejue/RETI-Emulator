#include "../include/core_debug.h"
#include "../include/dma.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void assert_sram_value(uint32_t idx, const char *expected) {
  Box *sram_boxes[] = {&sram_c_box, &sram_d_box, &sram_s_box};
  char prefix[64];
  snprintf(prefix, sizeof(prefix), "%02u: %s", idx, expected);
  for (size_t i = 0; i < sizeof(sram_boxes) / sizeof(sram_boxes[0]); i++) {
    char row[40];
    assert(mvwinnstr(sram_boxes[i]->win, idx + 1, 1, row, 38) != ERR);
    assert(strncmp(row, prefix, strlen(prefix)) == 0);
    assert(row[strlen(prefix)] == ' ' || row[strlen(prefix)] == '<');
  }
}

static void transcode(bool halted) {
  assert(ungetch(halted ? 'q' : 'n') == OK);
  assert(ungetch('t') == OK);
  if (halted) {
    wait_for_tui_quit();
  } else {
    evaluate_keyboard_input();
  }
}

static void test_transcode_cycle(bool halted) {
  const char *expected[][4] = {
      {"-1", "4294967295", "-1", "-1"},
      {"65", "65", "'A'", "ADDI PC 65"},
      {"0", "0", "NUL", "ADDI PC 0"},
      {"127", "127", "DEL", "ADDI PC 127"},
      {"128", "128", "128", "ADDI PC 128"},
      {"-2147483648", "2147483648", "-2147483648", "STORE PC 0"},
  };
  for (size_t mode = 0; mode < 4; mode++) {
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++) {
      assert_sram_value(i + 3, expected[i][mode]);
    }
    assert_sram_value(0, "4294967295");
    assert_sram_value(1, "ADDI PC 65");
    assert_sram_value(2, "ADDI PC 65");
    transcode(halted);
  }
  assert_sram_value(3, "-1");
  assert_sram_value(4, "65");
}

int main(void) {
  alarm(15);
  FILE *input = tmpfile();
  FILE *output = tmpfile();
  assert(input != NULL && output != NULL);
  SCREEN *screen = newterm("xterm", output, input);
  assert(screen != NULL);
  assert(resizeterm(40, 160) == OK);
  noecho();
  keypad(stdscr, TRUE);
  calculate_tui_layout(40, 160);
  for (uint8_t i = 0; i < NUM_BOXES; i++) {
    Box *box = boxes[i];
    box->win = newwin(box->height, box->width, box->y, box->x);
    assert(box->win != NULL);
  }

  regs = calloc(NUM_REGISTERS, sizeof(*regs));
  eprom = calloc(EPROM_SIZE, sizeof(*eprom));
  uart = calloc(NUM_PERIPHERY_ADDRESSES, sizeof(*uart));
  sram = tmpfile();
  assert(regs != NULL && eprom != NULL && uart != NULL && sram != NULL);
  uart[2] = 3;
  sram_size = 16;
  regs[PC] = regs[CS] = (SRAM_CONST << 30) | 2;
  regs[DS] = (SRAM_CONST << 30) | 3;
  regs[SP] = (SRAM_CONST << 30) | 15;
  set_sram_debug_sections((Program_Sections){
      .exists = true,
      .codesegment_start = 2,
      .has_interrupt_service_routines_start = true,
      .interrupt_service_routines_start = 1,
  });
  const uint32_t values[] = {UINT32_MAX, 65, 65, UINT32_MAX,
                             65, 0, 127, 128, (uint32_t)INT32_MIN};
  for (size_t i = 0; i < sram_size; i++) {
    write_file(sram, i, i < sizeof(values) / sizeof(values[0]) ? values[i] : 0);
  }
  assert(draw_tui());

  test_transcode_cycle(false);
  test_transcode_cycle(true);
  ds_vals_unsigned = true;
  assert(draw_tui());
  for (size_t mode = 0; mode < 4; mode++) {
    assert_sram_value(3, "4294967295");
    transcode(false);
  }

  free(regs);
  free(eprom);
  free(uart);
  fclose(sram);
  fin_tui();
  delscreen(screen);
  fclose(input);
  fclose(output);
  alarm(0);
  return 0;
}

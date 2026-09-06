#include "../include/core_debug.h"
#include "../include/dma.h"
#include "../include/exception.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void queue_input(const char *input) {
  for (size_t i = strlen(input); i > 0; i--) {
    assert(ungetch((unsigned char)input[i - 1]) == OK);
  }
}

static void run_debug_input(const char *input) {
  assert(ungetch('n') == OK);
  queue_input(input);
  evaluate_keyboard_input();
}

static void select_box(BoxIdentifier target) {
  static BoxIdentifier selected = EPROM_BOX;
  while (selected != target) {
    run_debug_input("\t");
    selected = (selected + 1) % 6;
  }
}

static void test_cpu_register_selection_and_values(void) {
  select_box(REGS_BOX);
  for (Register reg = PC; reg <= DS; reg++) {
    uint32_t previous[NUM_REGISTERS];
    memcpy(previous, regs, sizeof(previous));
    assert(ungetch('n') == OK);
    queue_input("4294967295\n");
    assert(ungetch(KEY_ENTER) == OK);
    assert(ungetch(KEY_UP) == OK);
    for (int i = 0; i <= reg; i++) {
      assert(ungetch(KEY_DOWN) == OK);
    }
    assert(ungetch('A') == OK);
    evaluate_keyboard_input();
    previous[reg] = UINT32_MAX;
    assert(memcmp(previous, regs, sizeof(previous)) == 0);
  }

  run_debug_input("Ajjj\n-2147483648\n");
  assert(regs[ACC] == (uint32_t)INT32_MIN);
  run_debug_input("Ajjj\n'q'\n");
  assert(regs[ACC] == 'q');
}

static void test_periphery_assignment_and_register_handlers(void) {
  select_box(UART_BOX);
  run_debug_input("A0\n65\n");
  assert(uart[0] == 65);
  run_debug_input("A1\n66\n");
  assert(uart[1] == 66);
  run_debug_input("A2\n3\n");
  assert(uart[2] == 3);

  for (uint8_t idx = INTERRUPT_CONTROLLER_ISR_BASE; idx < SYSTEM_INFO_BASE;
       idx++) {
    char input[24];
    snprintf(input, sizeof(input), "A%u\n7\n", idx);
    run_debug_input(input);
    assert(uart[idx] == 7);
  }
  assert(device_to_isr[INTERRUPT_TIMER] == 7);
  assert(device_to_prio[UART_DEVICE] == 7);

  timer_cnt = 5;
  run_debug_input("A9\n4294967295\n");
  assert(interrupt_timer_interval == UINT32_MAX);
  assert(timer_cnt == 0);
  run_debug_input("A10\n1234\n");
  assert(stack_heap_boundary == 1234);
  run_debug_input("A11\n2\n");
  assert(cpu_exception_cause == CPU_EXCEPTION_NONE);

  run_debug_input("A12\n1\n");
  assert(dma_is_active());
  run_debug_input("A13\n1073741825\n");
  assert(read_dma_register(DMA_SOURCE_REGISTER) == ((UART_CONST << 30) | 1));
  run_debug_input("A14\n2147483658\n");
  assert(read_dma_register(DMA_DESTINATION_REGISTER) ==
         ((SRAM_CONST << 30) | 10));
  run_debug_input("A15\n2\n");
  assert(read_dma_register(DMA_WORD_COUNT_REGISTER) == 2);
  run_debug_input("A16\n1\n");
  assert(read_dma_register(DMA_STATUS_REGISTER) == DMA_STATUS_BUSY);
  run_debug_input("A16\n0\n");
  assert(read_dma_register(DMA_STATUS_REGISTER) == DMA_STATUS_IDLE);

  for (int view = 0; view < 4; view++) {
    run_debug_input("aA9\n42\n");
    assert(interrupt_timer_interval == 42);
    interrupt_timer_interval = 0;
  }
}

static void test_invalid_input_can_retry_or_abort(void) {
  select_box(UART_BOX);
  const char *invalid_indices[] = {"17", "-1", "", "abc", "1.5", "4294967296"};
  for (size_t i = 0; i < sizeof(invalid_indices) / sizeof(invalid_indices[0]);
       i++) {
    char input[48];
    snprintf(input, sizeof(input), "A%s\n\n9\n123\n", invalid_indices[i]);
    interrupt_timer_interval = 0;
    run_debug_input(input);
    assert(interrupt_timer_interval == 123);
  }
  run_debug_input("A9\n4294967296\n\n25\n");
  assert(interrupt_timer_interval == 25);
  run_debug_input("A9\n-2147483649\nq");
  assert(interrupt_timer_interval == 25);
  run_debug_input("A17\n\033");
  assert(interrupt_timer_interval == 25);
  run_debug_input("A9\n12x\n\033");
  assert(interrupt_timer_interval == 25);
}

static void test_abort_leaves_registers_and_memory_unchanged(void) {
  const char abort_keys[] = {'q', 27};
  for (size_t i = 0; i < sizeof(abort_keys); i++) {
    char input[32];
    uint32_t previous[NUM_REGISTERS];
    memcpy(previous, regs, sizeof(previous));
    select_box(REGS_BOX);
    snprintf(input, sizeof(input), "Aj%c", abort_keys[i]);
    run_debug_input(input);
    snprintf(input, sizeof(input), "Ajjj\n123%c", abort_keys[i]);
    run_debug_input(input);
    assert(memcmp(previous, regs, sizeof(previous)) == 0);

    select_box(UART_BOX);
    interrupt_timer_interval = 37;
    snprintf(input, sizeof(input), "A9%c", abort_keys[i]);
    run_debug_input(input);
    snprintf(input, sizeof(input), "A9\n123%c", abort_keys[i]);
    run_debug_input(input);
    assert(interrupt_timer_interval == 37);

    select_box(SRAM_D_BOX);
    regs[DS] = SRAM_CONST << 30;
    write_file(sram, 0, 55);
    snprintf(input, sizeof(input), "A123%c", abort_keys[i]);
    run_debug_input(input);
    assert(read_file(sram, 0) == 55);
    snprintf(input, sizeof(input), "ak\n123%c", abort_keys[i]);
    run_debug_input(input);
    assert(sram_d_watchbox.watchobject == DS);
    assert(sram_d_watchbox.watchobject_addr == NULL);
  }
}

static void test_shared_value_input_editing(void) {
  const struct {
    const char *input;
    uint32_t expected;
  } cases[] = {{"12\b3\n", 13},   {"99\0250\n", 0},  {"'1'\n", '1'},
               {"'\\n'\n", '\n'}, {"'\\t'\n", '\t'}, {"'\\\\'\n", '\\'},
               {"'\\''\n", '\''}, {"x\n", 'x'}};
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    uint32_t value = UINT32_MAX;
    queue_input(cases[i].input);
    assert(get_user_input(&value));
    assert(value == cases[i].expected);
  }
}

static void test_menus_show_abort_hint(FILE *output) {
  const Menu_Entry entries[] = {{"PC", PC}, {"ACC", ACC}};
  const char *hint = "(abort: 'q' or 'esc')";
  char row[161];
  queue_input("q");
  assert(display_popup_menu(entries, 2) == CANCEL);
  assert(mvwinnstr(curscr, (term_height - 5) / 2 + 3, 0, row, 160) != ERR);
  assert(strstr(row, hint) != NULL);

  werase(stdscr);
  wrefresh(stdscr);
  assert(fflush(output) == 0);
  long start = ftell(output);
  assert(start >= 0);
  char input[12];
  queue_input("q");
  assert(!display_input_box(input, "Enter a value:", 11));
  assert(fflush(output) == 0);
  long end = ftell(output);
  assert(end > start);
  char *rendered = calloc(end - start + 1, 1);
  assert(rendered != NULL);
  assert(fseek(output, start, SEEK_SET) == 0);
  assert(fread(rendered, 1, end - start, output) == end - start);
  assert(strstr(rendered, hint) != NULL);
  assert(fseek(output, end, SEEK_SET) == 0);
  free(rendered);
}

static void test_uart_input_abort_does_not_deliver_a_byte(void) {
  debug_mode = true;
  uart[1] = 99;
  uart[2] = 1;
  queue_input("q");
  update_uart();
  assert(input_idx == 0);
  assert(input_len == 0);
  assert(uart[1] == 99);
  assert(uart[2] == 1);
  queue_input("\033");
  update_uart();
  assert(uart[1] == 99);
  assert(uart[2] == 1);
  max_waiting_instrs = 0;
  queue_input("a\n");
  update_uart();
  assert(uart[1] == 'a');
  assert(uart[2] == 3);
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
  init_interrupt_controller();
  init_cpu_exceptions();
  init_dma();

  test_cpu_register_selection_and_values();
  test_periphery_assignment_and_register_handlers();
  test_invalid_input_can_retry_or_abort();
  test_abort_leaves_registers_and_memory_unchanged();
  test_shared_value_input_editing();
  test_menus_show_abort_hint(output);
  test_uart_input_abort_does_not_deliver_a_byte();

  free(regs);
  free(eprom);
  free(uart);
  free(uart_input);
  fclose(sram);
  fin_tui();
  delscreen(screen);
  fclose(input);
  fclose(output);
  alarm(0);
  return 0;
}

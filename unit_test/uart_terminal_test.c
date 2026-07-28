#include "../include/assemble.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include "../include/uart.h"
#include "../include/uart_terminal.h"
#include <assert.h>
#include <stdint.h>
#include <unistd.h>

static void finish_uart_interrupt(void) {
  update_state(RETURN_FROM_INTERRUPT);
}

static void test_uart_key_triggers_configured_interrupt(void) {
  peripherals_dir = "/tmp";
  init_reti();

  set_interrupt_device_isr(UART_DEVICE, 2);
  uart[INTERRUPT_CONTROLLER_PRIO_BASE + UART_DEVICE] = 4;
  sync_interrupt_controller_from_memory();
  write_file(sram, 2, 20);
  write_array(regs, SP, (SRAM_CONST << 30) | 100, false);
  write_array(regs, PC, (SRAM_CONST << 30) | 50, false);

  assert(uart_interrupt_trigger('x'));
  assert(uart[1] == 'x');
  assert((uart[2] & 0b00000010) != 0);
  assert(read_array(regs, PC, false) == ((SRAM_CONST << 30) | 20));
  assert(stacked_isrs_cnt == 1);

  assert(!uart_interrupt_trigger('y'));
  finish_uart_interrupt();
  assert(stacked_isrs_cnt == 0);

  assert(uart_interrupt_trigger('y'));
  assert(uart[1] == 'y');
  finish_uart_interrupt();

  fin_reti();
}

static void test_non_debug_uart_terminal_reads_every_byte(void) {
  int input_pipe[2];
  assert(pipe(input_pipe) == 0);
  int saved_stdin = dup(STDIN_FILENO);
  assert(saved_stdin >= 0);
  assert(dup2(input_pipe[0], STDIN_FILENO) >= 0);

  peripherals_dir = "/tmp";
  init_reti();
  set_interrupt_device_isr(UART_DEVICE, 2);
  write_file(sram, 2, 20);
  write_array(regs, SP, (SRAM_CONST << 30) | 100, false);
  write_array(regs, PC, (SRAM_CONST << 30) | 50, false);

  assert(activate_uart_terminal());
  assert(uart_terminal_is_active());
  assert(write(input_pipe[1], "x", 1) == 1);
  update_uart_terminal();
  assert(uart[1] == 'x');
  finish_uart_interrupt();

  assert(write(input_pipe[1], "\x1b", 1) == 1);
  update_uart_terminal();
  assert(uart[1] == '\x1b');
  assert(uart_terminal_is_active());
  finish_uart_interrupt();
  close_uart_terminal();
  assert(!uart_terminal_is_active());

  fin_reti();
  assert(dup2(saved_stdin, STDIN_FILENO) >= 0);
  close(saved_stdin);
  close(input_pipe[0]);
  close(input_pipe[1]);
}

int main(void) {
  test_uart_key_triggers_configured_interrupt();
  test_non_debug_uart_terminal_reads_every_byte();
  return 0;
}

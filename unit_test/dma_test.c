#include "../include/assemble.h"
#include "../include/dma.h"
#include "../include/interrupt_controller.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include "../include/uart.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static uint32_t dma_address(uint32_t register_index) {
  return (UART_CONST << 30) | register_index;
}

static void test_dma_can_be_activated_through_memory(void) {
  dma_enabled = false;
  peripherals_dir = "/tmp";
  init_reti();

  assert(read_storage(dma_address(DMA_ACTIVE_REGISTER)) == 0);
  assert(dma_last_mapped_register() == DMA_ACTIVE_REGISTER);

  write_storage(dma_address(DMA_ACTIVE_REGISTER), 1);
  assert(read_storage(dma_address(DMA_ACTIVE_REGISTER)) == 1);
  assert(dma_last_mapped_register() == DMA_STATUS_REGISTER);

  fin_reti();
}

static void test_dma_copies_uart_words_and_interrupts(void) {
  const uint8_t input[] = {0x12, 0x34, 0x56, 0x78,
                           0x9a, 0xbc, 0xde, 0xf0};
  dma_enabled = true;
  peripherals_dir = "/tmp";
  init_reti();

  free(uart_input);
  uart_input = malloc(sizeof(input));
  assert(uart_input != NULL);
  memcpy(uart_input, input, sizeof(input));
  input_len = sizeof(input);
  input_idx = 0;

  set_interrupt_device_isr(CUSTOM, 4);
  uart[INTERRUPT_CONTROLLER_PRIO_BASE + CUSTOM] = 2;
  sync_interrupt_controller_from_memory();
  write_file(sram, 4, 20);
  write_array(regs, SP, (SRAM_CONST << 30) | 100, false);
  write_array(regs, PC, (SRAM_CONST << 30) | 50, false);

  write_storage(dma_address(DMA_SOURCE_REGISTER),
                (UART_CONST << 30) | 1);
  write_storage(dma_address(DMA_DESTINATION_REGISTER),
                (SRAM_CONST << 30) | 30);
  write_storage(dma_address(DMA_WORD_COUNT_REGISTER), 2);
  write_storage(dma_address(DMA_STATUS_REGISTER), DMA_STATUS_BUSY);

  update_dma();
  assert(read_file(sram, 30) == 0x12345678);
  assert(read_storage(dma_address(DMA_STATUS_REGISTER)) == DMA_STATUS_BUSY);

  update_dma();
  assert(read_file(sram, 31) == 0x9abcdef0);
  assert(read_storage(dma_address(DMA_STATUS_REGISTER)) ==
         DMA_STATUS_COMPLETE);
  assert(dma_interrupt_check());
  assert(read_array(regs, PC, false) == ((SRAM_CONST << 30) | 20));
  assert(stacked_isrs_cnt == 1);
  update_state(RETURN_FROM_INTERRUPT);

  fin_reti();
}

int main(void) {
  test_dma_can_be_activated_through_memory();
  test_dma_copies_uart_words_and_interrupts();
  return 0;
}

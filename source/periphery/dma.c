#include "../../include/dma.h"
#include "../../include/interrupt.h"
#include "../../include/interrupt_controller.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include "../../include/uart.h"

static bool dma_active = false;
static uint32_t dma_source = 0;
static uint32_t dma_destination = 0;
static uint32_t dma_word_count = 0;
static uint32_t dma_words_copied = 0;
static uint32_t dma_status = DMA_STATUS_IDLE;
static bool dma_interrupt_pending = false;

void init_dma(void) {
  dma_active = dma_enabled;
  dma_source = 0;
  dma_destination = 0;
  dma_word_count = 0;
  dma_words_copied = 0;
  dma_status = DMA_STATUS_IDLE;
  dma_interrupt_pending = false;
}

bool dma_is_active(void) { return dma_active; }

uint16_t dma_last_mapped_register(void) {
  return dma_active ? DMA_STATUS_REGISTER : DMA_ACTIVE_REGISTER;
}

uint32_t read_dma_register(uint16_t address) {
  if (address == DMA_ACTIVE_REGISTER) {
    return dma_active;
  }
  if (!dma_active) {
    return 0;
  }

  switch (address) {
  case DMA_SOURCE_REGISTER:
    return dma_source;
  case DMA_DESTINATION_REGISTER:
    return dma_destination;
  case DMA_WORD_COUNT_REGISTER:
    return dma_word_count;
  case DMA_STATUS_REGISTER:
    return dma_status;
  default:
    return 0;
  }
}

static bool dma_configuration_is_valid(void) {
  uint32_t uart_receive_address = (UART_CONST << 30) | 1;
  uint32_t destination_index = dma_destination & 0x7fffffff;

  return dma_source == uart_receive_address &&
         (dma_destination >> 30) >= SRAM_CONST &&
         destination_index <= sram_size &&
         dma_word_count <= sram_size - destination_index;
}

static void complete_dma(uint32_t status) {
  dma_status = status;
  dma_interrupt_pending = true;
}

static void start_dma(void) {
  dma_words_copied = 0;
  dma_interrupt_pending = false;
  if (!dma_configuration_is_valid()) {
    complete_dma(DMA_STATUS_ERROR);
    return;
  }

  dma_status = DMA_STATUS_BUSY;
  if (dma_word_count == 0) {
    complete_dma(DMA_STATUS_COMPLETE);
  }
}

void write_dma_register(uint16_t address, uint32_t value) {
  if (address == DMA_ACTIVE_REGISTER) {
    dma_active = value != 0;
    if (!dma_active) {
      dma_status = DMA_STATUS_IDLE;
      dma_interrupt_pending = false;
    }
    return;
  }
  if (!dma_active ||
      (dma_status == DMA_STATUS_BUSY && address != DMA_STATUS_REGISTER)) {
    return;
  }

  switch (address) {
  case DMA_SOURCE_REGISTER:
    dma_source = value;
    break;
  case DMA_DESTINATION_REGISTER:
    dma_destination = value;
    break;
  case DMA_WORD_COUNT_REGISTER:
    dma_word_count = value;
    break;
  case DMA_STATUS_REGISTER:
    if (value == DMA_STATUS_BUSY) {
      start_dma();
    } else if (value == DMA_STATUS_IDLE) {
      dma_status = DMA_STATUS_IDLE;
      dma_interrupt_pending = false;
    }
    break;
  }
}

void update_dma(void) {
  uint32_t word;

  if (dma_status != DMA_STATUS_BUSY) {
    return;
  }
  if (!uart_dma_read_word(&word)) {
    complete_dma(DMA_STATUS_ERROR);
    return;
  }

  write_storage(dma_destination + dma_words_copied, word);
  dma_words_copied++;
  if (dma_words_copied == dma_word_count) {
    complete_dma(DMA_STATUS_COMPLETE);
  }
}

bool dma_interrupt_check(void) {
  if (!dma_interrupt_pending) {
    return false;
  }

  sync_interrupt_controller_from_memory();
  uint8_t dma_isr = device_to_isr[CUSTOM];
  if (dma_isr == INVALID_ISR_NUM) {
    return false;
  }

  dma_interrupt_pending = false;
  in.arg8 = dma_isr;
  update_state(HARDWARE_INTERRUPT);
  return out.retbool1;
}

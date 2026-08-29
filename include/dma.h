#ifndef DMA_H
#define DMA_H

#include <stdbool.h>
#include <stdint.h>

#define DMA_ACTIVE_REGISTER 12
#define DMA_SOURCE_REGISTER 13
#define DMA_DESTINATION_REGISTER 14
#define DMA_WORD_COUNT_REGISTER 15
#define DMA_STATUS_REGISTER 16
#define NUM_PERIPHERY_ADDRESSES (DMA_STATUS_REGISTER + 1)

#define DMA_STATUS_IDLE 0
#define DMA_STATUS_BUSY 1
#define DMA_STATUS_COMPLETE 2
#define DMA_STATUS_ERROR 3

void init_dma(void);
bool dma_is_active(void);
uint16_t dma_last_mapped_register(void);
uint32_t read_dma_register(uint16_t address);
void write_dma_register(uint16_t address, uint32_t value);
void update_dma(void);
bool dma_interrupt_check(void);

#endif // DMA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifndef RETI_H
#define RETI_H

extern uint32_t ivt_max_idx;
extern uint32_t num_instrs_prgrm;
extern uint32_t num_instrs_start_prgrm;
extern uint32_t num_instrs_isrs;

extern uint32_t *regs;
extern uint32_t *eprom;
extern uint8_t *uart;
extern FILE *sram;
// extern FILE *hdd;

extern char *eprom_watchpoint;
extern char *sram_watchpoint_cs;
extern char *sram_watchpoint_ds;
extern char *sram_watchpoint_stack;
/*extern uint64_t hdd_watchpoint;*/

#define ADJUSTEED_EPROM_PRGRM_SIZE 14
#define NUM_REGISTERS 8
#define NUM_UART_ADDRESSES 3

#define EPROM_CONST 0b00
#define UART_CONST 0b01
#define SRAM_CONST 0b10

void load_adjusted_eprom_prgrm();

uint32_t read_file(FILE *dev, uint64_t address);
void write_file(FILE *dev, uint64_t address, uint32_t buffer);

uint32_t read_array(void *stor, uint16_t addr, bool is_uart);
void write_array(void *stor, uint16_t addr, uint32_t buffer, bool is_uart);

uint32_t read_storage_ds_fill(uint32_t addr);
uint32_t read_storage_sram_constant_fill(uint32_t addr) ;
uint32_t read_storage(uint32_t addr);
void write_storage_ds_fill(uint64_t addr, uint32_t buffer);
void write_storage(uint32_t addr, uint32_t buffer);

void init_reti();
void fin_reti();

#endif // RETI_H

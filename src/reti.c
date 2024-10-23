#include "../include/reti.h"
#include "../include/assemble.h"
#include "../include/debug.h"
#include "../include/parse_args.h"
#include "../include/utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

uint32_t *regs, *eprom;
uint8_t *uart;

FILE *sram, *hdd;

uint32_t ivt_max_idx = -1;
uint32_t num_instrs_prgrm = 0;
uint32_t num_instrs_start_prgrm = 0;
uint32_t num_instrs_isrs = 0;

char *eprom_watchpoint = "PC";
char *sram_watchpoint_cs = "PC";
char *sram_watchpoint_ds = "DS";
char *sram_watchpoint_stack = "SP";

uint64_t hdd_watchpoint = 0;

void init_reti() {
  regs = malloc(sizeof(uint32_t) * NUM_REGISTERS);
  // TODO: herausfinden, wie man num_instrs_start_prgrm vorher bestimmt
  if (strcmp(eprom_prgrm_path, "") == 0) {
    num_instrs_start_prgrm = ADJUSTEED_EPROM_PRGRM_SIZE;
    eprom = malloc(sizeof(uint32_t) * num_instrs_start_prgrm);
  } else {
    eprom = NULL;
  }
  uart = malloc(sizeof(uint32_t) * NUM_UART_ADDRESSES);

  memset(regs, 0, sizeof(uint32_t) * NUM_REGISTERS);
  memset(uart, 0, sizeof(uint32_t) * NUM_UART_ADDRESSES);

  // TODO: Tobias: Die ganzen Speicher nicht mit 0 initialisiert
  char *file_path = proper_str_cat(peripherals_dir, "/sram.bin");
  sram = fopen(file_path, "w+b");

  file_path = proper_str_cat(peripherals_dir, "/hdd.bin");
  hdd = fopen(file_path, "w+b");

  if (!sram || !hdd) {
    fprintf(stderr, "Failed to open storage files");
    exit(EXIT_FAILURE);
  }

  uart[2] = 0b00000011;
}

void load_adjusted_eprom_prgrm() {
  uint8_t i = 0;
  String_Instruction str_instr = {
      .op = "LOADI", .opd1 = "SP", .opd2 = "-1", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr = (String_Instruction){
      .op = "MULTI", .opd1 = "SP", .opd2 = "1024", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr = (String_Instruction){
      .op = "ADDI", .opd1 = "SP", .opd2 = "1023", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  str_instr = (String_Instruction){
      .op = "MOVE", .opd1 = "SP", .opd2 = "BAF", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  // Codesegment register
  uint32_t im = 0b10 << 30 | num_instrs_isrs;
  // or because of assumption that num_instrs_isrs is less than 2^30
  uint32_t im_upper_str = sign_extend_22_to_32(im >> 10);
  uint32_t im_lower_str = im & IMMEDIATE_MASK;
  str_instr =
      (String_Instruction){.op = "LOADI", .opd1 = "CS", .opd2 = "", .opd3 = ""};
  strcpy(str_instr.opd2, mem_value_to_str(im_upper_str, false));
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr = (String_Instruction){
      .op = "MULTI", .opd1 = "CS", .opd2 = "1024", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr =
      (String_Instruction){.op = "ORI", .opd1 = "CS", .opd2 = "", .opd3 = ""};
  strcpy(str_instr.opd2, mem_value_to_str(im_lower_str, false));
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  str_instr = (String_Instruction){
      .op = "MOVE", .opd1 = "CS", .opd2 = "DS", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  // Datensegment register
  im_upper_str = sign_extend_22_to_32(num_instrs_prgrm >> 10);
  im_lower_str = num_instrs_prgrm & IMMEDIATE_MASK;
  str_instr = (String_Instruction){
      .op = "LOADI", .opd1 = "ACC", .opd2 = "", .opd3 = ""};
  strcpy(str_instr.opd2, mem_value_to_str(im_upper_str, false));
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr = (String_Instruction){
      .op = "MULTI", .opd1 = "ACC", .opd2 = "1024", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr =
      (String_Instruction){.op = "ORI", .opd1 = "ACC", .opd2 = "", .opd3 = ""};
  strcpy(str_instr.opd2, mem_value_to_str(im_lower_str, false));
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);
  str_instr = (String_Instruction){
      .op = "ADD", .opd1 = "DS", .opd2 = "ACC", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  str_instr = (String_Instruction){
      .op = "LOADI", .opd1 = "ACC", .opd2 = "0", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  str_instr = (String_Instruction){
      .op = "MOVE", .opd1 = "CS", .opd2 = "PC", .opd3 = ""};
  write_array(eprom, i++, assembly_to_machine(&str_instr), false);

  // TODO: Tobias, soll eprom schreiben ab hier gelockt sein?
}

uint32_t read_array(void *stor, uint16_t addr, bool is_uart) {
  if (is_uart) {
    if (!(uart[2] & 0b00000010) && addr == 1) {
      fprintf(stderr, "Warning: No new data in the receive register");
    } else if (addr == 0) {
      fprintf(stderr, "Warning: Reading from the send register of the UART makes no sense");
    }
    // uart[2] = uart[2] & 0b11111101; has to be done by the programmer
    return ((uint8_t *)stor)[addr];
  } else {
    return ((uint32_t *)stor)[addr];
  }
}

void write_array(void *stor, uint16_t addr, uint32_t buffer, bool is_uart) {
  if (is_uart) {
    if (!(uart[2] & 0b00000001) && addr == 0) {
      // TODO: Tobias fragen, ob er damit agreed
      fprintf(stderr, "Warning: UART does not accept any further data");
    } else if (!(uart[2] & 0b00000001) && addr == 2 && (buffer & 0b00000001)) {
      fprintf(stderr, "Warning: Only the UART should allow sending again");
    } else if (!(uart[2] & 0b00000010) && addr == 2 && (buffer & 0b00000010)) {
      fprintf(stderr, "Warning: Only the UART itself should tell that it received something");
    } else if (addr == 1) {
      fprintf(stderr, "Warning: Writing to the receive register of the UART makes no sense");
    }
    ((uint8_t *)stor)[addr] = buffer & 0xFF;
  } else {
    ((uint32_t *)stor)[addr] = buffer;
  }
}

uint32_t read_file(FILE *dev, uint64_t address) {
  uint32_t big_endian_buffer;
  fseek(dev, address * sizeof(uint32_t), SEEK_SET);
  fread(&big_endian_buffer, sizeof(uint32_t), 1, dev);
  return swap_endian_32(big_endian_buffer);
}

void write_file(FILE *dev, uint64_t addr, uint32_t buffer) {
  uint32_t big_endian_buffer = swap_endian_32(buffer);
  fseek(dev, addr * sizeof(uint32_t), SEEK_SET);
  fwrite(&big_endian_buffer, sizeof(uint32_t), 1, dev);
  fflush(dev);
}

uint32_t read_storage_ds_fill(uint32_t addr) {
  addr = addr | (read_array(regs, DS, false) & 0xffc00000);
  return read_storage(addr);
}

uint32_t read_storage_sram_constant_fill(uint32_t addr) {
  addr = addr | 0x80000000;
  return read_storage(addr);
}

uint32_t read_storage(uint32_t addr) {
  uint8_t stor_mode = addr >> 30;
  switch (stor_mode) {
  case EPROM_CONST:
    // addr = addr & 0x3FFFFFFF; makes no sense because it already is 0b00
    return read_array(eprom, addr, false);
    break;
  case UART_CONST:
    addr = addr & 0x3FFFFFFF;
    return read_array(uart, addr, true);
    break;
  default: // SRAM_CONST
    addr = addr & 0x7FFFFFFF;
    return read_file(sram, addr);
    break;
  }
}

void write_storage_ds_fill(uint64_t addr, uint32_t buffer) {
  addr = addr | (read_array(regs, DS, false) & 0xffc00000);
  write_storage(addr, buffer);
}

void write_storage(uint32_t addr, uint32_t buffer) {
  uint8_t stor_mode = addr >> 30;
  switch (stor_mode) {
  case EPROM_CONST:
    // addr = addr & 0x3FFFFFFF; makes no sense because it already is 0b00
    write_array(eprom, addr, buffer, false);
    break;
  case UART_CONST:
    addr = addr & 0x3FFFFFFF;
    write_array(uart, addr, buffer, true);
    break;
  default: // SRAM_CONST
    addr = addr & 0x7FFFFFFF;
    write_file(sram, addr, buffer);
    break;
  }
}

void fin_reti() {
  fclose(sram);
  fclose(hdd);
}

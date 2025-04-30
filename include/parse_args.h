#include <stdbool.h>
#include <stdint.h>

#ifndef PARSE_ARGS_H
#define PARSE_ARGS_H

extern uint32_t sram_size;
extern uint16_t page_size;
// extern uint32_t hdd_size;
extern bool debug_mode;
extern bool test_mode;
extern bool binary_mode;
extern bool extended_features;
extern bool read_metadata;
extern uint8_t radius;
extern uint8_t max_waiting_instrs;
extern bool verbose;
extern bool legacy_debug_tui;
extern bool ds_vals_unsigned;
extern bool ds_address_extension;

extern char *peripherals_dir;
extern char *eprom_prgrm_path;
extern char *sram_prgrm_path;
extern char *isrs_prgrm_path;

void parse_args(uint8_t argc, char *argv[]);
void print_args() ;

#endif // PARSE_ARGS_H

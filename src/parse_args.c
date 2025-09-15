#include "../include/parse_args.h"
#include "../include/interpr.h"
#include "../include/interrupt.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

uint32_t sram_size = 65536;
uint16_t page_size = 4096;
bool debug_mode = false;
bool test_mode = false;
bool binary_mode = false;
bool extended_features = false;
bool read_metadata = false;
uint8_t max_waiting_instrs = 10;
bool verbose = false;
bool ds_vals_unsigned = false;

char *peripherals_dir = ".";
char *eprom_prgrm_path = "";
char *sram_prgrm_path = "";
char *isrs_prgrm_path = "";

void print_help(char *bin_name) {
  fprintf(
      stderr,
      "Usage: %s -r ram_size -p page_size -d (daemon mode) "
      "-f file_dir -e eprom_prgrm_path -i isrs_prgrm_path "
      "-w max_waiting_instrs -t (test mode) -m (read metadata) -v (verbose) "
      "-b (binary mode) -E (extended features) -a (all) -u (ds vals unsigned) "
      "-I timer_interrupt_interval -h (help page) "
      "prgrm_path\n",
      bin_name);
}

void parse_args(uint8_t argc, char *argv[]) {
  uint32_t opt;

  while ((opt = getopt(argc, argv, "s:p:f:e:i:w:hdvtmbEauI:")) != -1) {
    char *endptr;
    int64_t tmp_val;

    switch (opt) {
    case 'a':
      debug_mode = true;
      peripherals_dir = "/tmp";
      verbose = true;
      read_metadata = true;
      binary_mode = true;
      break;
    case 's':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid sram size\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        fprintf(stderr,
                "Error: SRAM max index must be between 0 and 4294967295\n");
        exit(EXIT_FAILURE);
      }
      sram_size = tmp_val;
      break;
    case 'p':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid page size\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT16_MAX) {
        fprintf(stderr, "Error: Page size must be between 0 and 65535\n");
        exit(EXIT_FAILURE);
      }
      page_size = tmp_val;
      break;
    case 'd':
      debug_mode = true;
      break;
    case 'f':
      peripherals_dir = optarg;
      break;
    case 'e':
      eprom_prgrm_path = optarg;
      break;
    case 'i':
      isrs_prgrm_path = optarg;
      break;
    case 'w':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid max waiting instructions\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        fprintf(stderr,
                "Error: Invalid max waiting must be between 0 and 4294967296\n");
        exit(EXIT_FAILURE);
      }
      max_waiting_instrs = tmp_val;
      break;
    case 'v':
      verbose = true;
      break;
    case 't':
      test_mode = true;
      break;
    case 'm':
      read_metadata = true;
      break;
    case 'b':
      binary_mode = true;
      break;
    case 'E':
      extended_features = true;
      break;
    case 'u':
      ds_vals_unsigned = true;
      break;
    case 'h':
      print_help(argv[0]);
      exit(EXIT_SUCCESS);
    case 'I':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid interrupt timer interval\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        fprintf(stderr,
                "Error: Interrupt timer interval must be between 0 and 4294967296\n");
        exit(EXIT_FAILURE);
      }
      interrupt_timer_interval = tmp_val;
      break;
    default:
      print_help(argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  if (optind >= argc) {
    fprintf(stderr, "Expected argument after options\n");
    print_help(argv[0]);
    exit(EXIT_FAILURE);
  }

  sram_prgrm_path = argv[optind];
}

void print_args() {
  printf("SRAM size: %u\n", sram_size);
  printf("Page size: %u\n", page_size);
  printf("Maximum number of waiting instructions: %u\n", max_waiting_instrs);
  printf("Interrupt timer interval: %u\n", interrupt_timer_interval);
  printf("Debug mode: %s\n", debug_mode ? "true" : "false");
  printf("Read metadata: %s\n", read_metadata ? "true" : "false");
  printf("Test mode: %s\n", test_mode ? "true" : "false");
  printf("Binary mode: %s\n", binary_mode ? "true" : "false");
  printf("Verbose: %s\n", verbose ? "true" : "false");
  printf("Datasegment values unsigned: %s\n",
         ds_vals_unsigned ? "true" : "false");
  printf("Extended features: %s\n", extended_features ? "true" : "false");
  printf("Peripheral file directory: %s\n", peripherals_dir);
  printf("Eprom program path: %s\n", eprom_prgrm_path);
  printf("Interrupt service routines program path: %s\n", isrs_prgrm_path);
  printf("SRAM program path: %s\n", sram_prgrm_path);
}

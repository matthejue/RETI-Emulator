#include "../include/parse_args.h"
#include "../include/globals.h"
#include "../include/interpr.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

uint32_t sram_size = 4294967295;
uint16_t page_size = 4096;
uint32_t hdd_size = 4294967295;
bool daemon_mode = false;
bool test_mode = false;
bool read_metadata = false;
uint8_t radius = 2;
uint8_t max_waiting_instrs = 10;
bool verbose = false;

char *peripherals_dir = ".";
char *eprom_prgrm_path = "";
char *sram_prgrm_path = "";
char *isrs_prgrm_path = "";

void print_help(char *bin_name) {
  fprintf(stderr,
          "Usage: %s -r ram_size -p page_size -H hdd_size -d (daemon mode) "
          "-r radius -f file_dir -e eprom_prgrm_path -i isrs_prgrm_path "
          "-w max_waiting_instrs -t (test mode) -m (read metadata) -v "
          "(verbose) -h (help page) "
          "prgrm_path\n",
          bin_name);
}

void parse_args(uint8_t argc, char *argv[]) {
  uint32_t opt;

  while ((opt = getopt(argc, argv, "s:p:H:dr:f:e:i:w:hvtm")) != -1) {
    char *endptr;
    long tmp_val;

    switch (opt) {
    case 's':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        perror("Error: Invalid sram size");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        perror("Error: SRAM max index must be between 0 and 4294967295");
        exit(EXIT_FAILURE);
      }
      sram_size = tmp_val;
      break;
    case 'p':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        perror("Error: Invalid page size");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT16_MAX) {
        perror("Error: Page size must be between 0 and 65535");
        exit(EXIT_FAILURE);
      }
      page_size = tmp_val;
      break;
    case 'H':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        perror("Error: Invalid hdd size");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        perror("Error: HDD max index must be between 0 and 4294967295");
        exit(EXIT_FAILURE);
      }
      hdd_size = tmp_val;
      break;
    case 'd':
      daemon_mode = true;
      break;
    case 'r':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        perror("Error: Invalid radius size");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT8_MAX) {
        perror("Error: Radius must be between 0 and 255");
        exit(EXIT_FAILURE);
      }
      radius = tmp_val;
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
        perror("Error: Invalid max waiting instructions");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT8_MAX) {
        perror("Error: Max waiting instructions must be between 0 and 255");
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
    case 'h':
      print_help(argv[0]);
      exit(EXIT_SUCCESS);
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
  printf("SRAM Size: %u\n", sram_size);
  printf("Page Size: %u\n", page_size);
  printf("HDD Size: %u\n", hdd_size);
  printf("Maximum number of waiting instructions: %u\n", max_waiting_instrs);
  printf("Daemon Mode: %s\n", daemon_mode ? "true" : "false");
  printf("Read Metadata: %s\n", read_metadata ? "true" : "false");
  printf("Test Mode: %s\n", test_mode ? "true" : "false");
  printf("Verbose: %s\n", verbose ? "true" : "false");
  printf("Radius: %u\n", radius);
  printf("Peripheral File Directory: %s\n", peripherals_dir);
  printf("Eprom Program Path: %s\n", eprom_prgrm_path);
  printf("Interrupt Service Routines Program Path: %s\n", isrs_prgrm_path);
  printf("SRAM Program Path: %s\n", sram_prgrm_path);
}

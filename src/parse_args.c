#include "../include/parse_args.h"
#include "../include/interpr.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

uint32_t sram_size = 65536;
uint16_t page_size = 4096;
uint32_t hdd_size = 4294967295;
bool debug_mode = false;
bool test_mode = false;
bool binary_mode = false;
bool extended_features = false;
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
          "(verbose) -b (binary mode) -E (extended features) -a (all) -h (help "
          "page) "
          "prgrm_path\n",
          bin_name);
}

void parse_args(uint8_t argc, char *argv[]) {
  uint32_t opt;

  while ((opt = getopt(argc, argv, "s:p:H:dr:f:e:i:w:hvtmbEa")) != -1) {
    char *endptr;
    long tmp_val;

    switch (opt) {
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
    case 'H':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid hdd size\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT32_MAX) {
        fprintf(stderr,
                "Error: HDD max index must be between 0 and 4294967295\n");
        exit(EXIT_FAILURE);
      }
      hdd_size = tmp_val;
      break;
    case 'd':
      debug_mode = true;
      break;
    case 'r':
      tmp_val = strtol(optarg, &endptr, 10);
      if (endptr == optarg || *endptr != '\0') {
        fprintf(stderr, "Error: Invalid radius size\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT8_MAX) {
        fprintf(stderr, "Error: Radius must be between 0 and 255\n");
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
        fprintf(stderr, "Error: Invalid max waiting instructions\n");
        exit(EXIT_FAILURE);
      }
      if (tmp_val < 0 || tmp_val > UINT8_MAX) {
        fprintf(stderr,
                "Error: Max waiting instructions must be between 0 and 255\n");
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
    case 'a':
      debug_mode = true;
      peripherals_dir = "/tmp";
      verbose = true;
      read_metadata = true;
      binary_mode = true;
      extended_features = true;
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
  printf("Daemon Mode: %s\n", debug_mode ? "true" : "false");
  printf("Read Metadata: %s\n", read_metadata ? "true" : "false");
  printf("Test Mode: %s\n", test_mode ? "true" : "false");
  printf("Binary Mode: %s\n", binary_mode ? "true" : "false");
  printf("Verbose: %s\n", verbose ? "true" : "false");
  printf("Radius: %u\n", radius);
  printf("Peripheral File Directory: %s\n", peripherals_dir);
  printf("Eprom Program Path: %s\n", eprom_prgrm_path);
  printf("Interrupt Service Routines Program Path: %s\n", isrs_prgrm_path);
  printf("SRAM Program Path: %s\n", sram_prgrm_path);
}

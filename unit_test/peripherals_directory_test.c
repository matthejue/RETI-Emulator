#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include <assert.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void test_peripherals_directory_defaults_to_current_directory(void) {
  char current_directory[PATH_MAX];
  assert(getcwd(current_directory, sizeof(current_directory)) != NULL);

  char directory_template[] = "/tmp/reti_emulator_default_test.XXXXXX";
  char *test_directory = mkdtemp(directory_template);
  assert(test_directory != NULL);
  assert(chdir(test_directory) == 0);

  peripherals_dir = ".";
  init_reti();
  assert(access(".reti_emulaor", F_OK) == 0);
  assert(access(".reti_emulaor/sram.bin", F_OK) == 0);

  fin_reti();
  assert(remove(".reti_emulaor/sram.bin") == 0);
  assert(rmdir(".reti_emulaor") == 0);
  assert(chdir(current_directory) == 0);
  assert(rmdir(test_directory) == 0);
}

static void test_peripherals_directory_uses_f_path(void) {
  char directory_template[] = "/tmp/reti_emulator_test.XXXXXX";
  char *peripherals_path = mkdtemp(directory_template);
  assert(peripherals_path != NULL);

  peripherals_dir = peripherals_path;
  init_reti();

  char *emulator_path =
      build_reti_emulator_directory_path(peripherals_path);
  char *sram_path = build_reti_emulator_file_path(peripherals_path, "sram.bin");
  assert(access(emulator_path, F_OK) == 0);
  assert(access(sram_path, F_OK) == 0);

  fin_reti();
  assert(remove(sram_path) == 0);
  assert(rmdir(emulator_path) == 0);
  assert(rmdir(peripherals_path) == 0);
  free(emulator_path);
  free(sram_path);
}

int main(void) {
  test_peripherals_directory_defaults_to_current_directory();
  test_peripherals_directory_uses_f_path();
  return 0;
}

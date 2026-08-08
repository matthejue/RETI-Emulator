#include "../../include/terminal_view.h"
#include "../../include/parse/parse_args.h"
#include "../../include/utils.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static int terminal_output_fd = -1;

bool init_terminal_output(void) {
  if (!ensure_reti_emulator_directory(peripherals_dir)) {
    return false;
  }

  char *output_path =
      build_reti_emulator_file_path(peripherals_dir, "terminal_output.bin");
  terminal_output_fd =
      open(output_path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
  free(output_path);
  return terminal_output_fd >= 0;
}

void append_terminal_output(uint8_t byte) {
  if (terminal_output_fd < 0) {
    return;
  }

  (void)write(terminal_output_fd, &byte, 1);
}

bool replay_terminal_output(void) {
  char *output_path =
      build_reti_emulator_file_path(peripherals_dir, "terminal_output.bin");
  FILE *output = fopen(output_path, "rb");
  free(output_path);
  if (output == NULL) {
    return false;
  }

  uint8_t buffer[4096];
  size_t len;
  bool success = true;
  while ((len = fread(buffer, 1, sizeof(buffer), output)) > 0) {
    if (fwrite(buffer, 1, len, stdout) != len) {
      success = false;
      break;
    }
  }
  if (ferror(output)) {
    success = false;
  }

  fclose(output);
  fflush(stdout);
  return success;
}

void close_terminal_output(void) {
  if (terminal_output_fd < 0) {
    return;
  }

  close(terminal_output_fd);
  terminal_output_fd = -1;
}

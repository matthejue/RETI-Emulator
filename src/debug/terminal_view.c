#include "../../include/terminal_view.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *TERMINAL_ROOT_DIR = "/tmp/reti_emulator";
static const char *TERMINAL_OUTPUT_PATH =
    "/tmp/reti_emulator/terminal_output.bin";

static int terminal_output_fd = -1;

static bool ensure_terminal_dir(void) {
  return mkdir(TERMINAL_ROOT_DIR, 0700) == 0 || errno == EEXIST;
}

bool init_terminal_output(void) {
  if (!ensure_terminal_dir()) {
    return false;
  }

  terminal_output_fd =
      open(TERMINAL_OUTPUT_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
  return terminal_output_fd >= 0;
}

void append_terminal_output(uint8_t byte) {
  if (terminal_output_fd < 0) {
    return;
  }

  (void)write(terminal_output_fd, &byte, 1);
}

bool replay_terminal_output(void) {
  FILE *output = fopen(TERMINAL_OUTPUT_PATH, "rb");
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

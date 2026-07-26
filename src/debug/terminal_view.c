#include "../../include/terminal_view.h"
#include "../../include/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static const char *TERMINAL_ROOT_DIR = "/tmp/reti_emulator";
static const char *TERMINAL_OUTPUT_PATH =
    "/tmp/reti_emulator/terminal_output.bin";
static const char *TERMINAL_INPUT_PATH =
    "/tmp/reti_emulator/terminal_input.bin";

static int terminal_output_fd = -1;
static int terminal_input_fd = -1;
static pid_t terminal_viewer_pid = -1;

static bool ensure_terminal_dir(void) {
  return mkdir(TERMINAL_ROOT_DIR, 0700) == 0 || errno == EEXIST;
}

bool init_terminal_output(void) {
  if (!ensure_terminal_dir()) {
    return false;
  }

  terminal_output_fd =
      open(TERMINAL_OUTPUT_PATH, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0600);
  terminal_input_fd =
      open(TERMINAL_INPUT_PATH, O_RDWR | O_CREAT | O_TRUNC | O_NONBLOCK, 0600);
  if (terminal_output_fd < 0 || terminal_input_fd < 0) {
    close_terminal_output();
    return false;
  }
  return true;
}

void append_terminal_output(uint8_t byte) {
  if (terminal_output_fd < 0) {
    return;
  }

  (void)write(terminal_output_fd, &byte, 1);
}

bool read_terminal_input(uint8_t *byte) {
  return terminal_input_fd >= 0 && read(terminal_input_fd, byte, 1) == 1;
}

void discard_terminal_input(void) {
  uint8_t byte;
  while (read_terminal_input(&byte)) {
  }
}

static void reap_terminal_viewer(void) {
  if (terminal_viewer_pid <= 0) {
    return;
  }

  pid_t wait_result = waitpid(terminal_viewer_pid, NULL, WNOHANG);
  if (wait_result == terminal_viewer_pid) {
    terminal_viewer_pid = -1;
  }
}

static void detach_terminal_viewer_from_tui(void) {
  int null_fd = open("/dev/null", O_RDWR);
  if (null_fd < 0) {
    return;
  }

  dup2(null_fd, STDIN_FILENO);
  dup2(null_fd, STDOUT_FILENO);
  dup2(null_fd, STDERR_FILENO);
  if (null_fd > STDERR_FILENO) {
    close(null_fd);
  }
}

bool start_terminal_viewer(void) {
  reap_terminal_viewer();
  if (terminal_viewer_pid > 0) {
    return true;
  }
  if (terminal_output_fd < 0) {
    return false;
  }

  char *script_path = build_debug_script_path("terminal_view.py");
  if (script_path == NULL || access(script_path, R_OK) != 0) {
    free(script_path);
    return false;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    free(script_path);
    return false;
  }

  if (child_pid == 0) {
    detach_terminal_viewer_from_tui();
    execlp("python3", "python3", script_path, TERMINAL_OUTPUT_PATH,
           TERMINAL_INPUT_PATH, NULL);
    _exit(EXIT_FAILURE);
  }

  free(script_path);
  terminal_viewer_pid = child_pid;
  return true;
}

void stop_terminal_viewer(void) {
  reap_terminal_viewer();
  if (terminal_viewer_pid <= 0) {
    return;
  }

  kill(terminal_viewer_pid, SIGTERM);
  waitpid(terminal_viewer_pid, NULL, 0);
  terminal_viewer_pid = -1;
}

void close_terminal_output(void) {
  if (terminal_output_fd >= 0) {
    close(terminal_output_fd);
    terminal_output_fd = -1;
  }

  if (terminal_input_fd >= 0) {
    close(terminal_input_fd);
    terminal_input_fd = -1;
  }
}

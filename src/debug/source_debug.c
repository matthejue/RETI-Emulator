#include "../../include/source_debug.h"
#include "../../include/assemble.h"
#include "../../include/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <unistd.h>

static pid_t source_debugger_pid = -1;

static const char *SOURCE_DEBUG_ROOT_DIR = "/tmp/reti_emulator";
static const char *SOURCE_DEBUG_STATE_PATH =
    "/tmp/reti_emulator/source_debug_state.bin";
static const char *SOURCE_DEBUG_STATE_TMP_PATH =
    "/tmp/reti_emulator/source_debug_state.bin.tmp";

static bool ensure_source_debug_dir(void) {
  return mkdir(SOURCE_DEBUG_ROOT_DIR, 0700) == 0 || errno == EEXIST;
}

static bool write_source_debug_state_file(uint32_t pc, uint32_t cs) {
  int state_fd =
      open(SOURCE_DEBUG_STATE_TMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (state_fd < 0) {
    return false;
  }

  uint32_t state_values[] = {pc, cs};
  ssize_t bytes_written =
      write(state_fd, state_values, sizeof(state_values));
  bool success = bytes_written == (ssize_t)sizeof(state_values) &&
                 fsync(state_fd) == 0;
  if (close(state_fd) != 0) {
    success = false;
  }
  if (success &&
      rename(SOURCE_DEBUG_STATE_TMP_PATH, SOURCE_DEBUG_STATE_PATH) != 0) {
    success = false;
  }
  if (!success) {
    unlink(SOURCE_DEBUG_STATE_TMP_PATH);
  }
  return success;
}

void write_source_debug_state(void) {
  if (regs == NULL || !ensure_source_debug_dir()) {
    return;
  }

  write_source_debug_state_file(read_array(regs, PC, false),
                                read_array(regs, CS, false));
}

static void reap_source_debugger_if_exited(void) {
  if (source_debugger_pid <= 0) {
    return;
  }

  pid_t wait_result = waitpid(source_debugger_pid, NULL, WNOHANG);
  if (wait_result == source_debugger_pid) {
    source_debugger_pid = -1;
  }
}

static char *build_debuginfo_path(void) {
  const char *last_slash = strrchr(sram_prgrm_path, '/');
  if (last_slash == NULL) {
    return strdup("debuginfo.json");
  }

  size_t dir_len = (size_t)(last_slash - sram_prgrm_path);
  size_t total_len = dir_len + strlen("/debuginfo.json") + 1;
  char *path = malloc(total_len);
  strncpy(path, sram_prgrm_path, dir_len);
  path[dir_len] = '\0';
  strcat(path, "/debuginfo.json");
  return path;
}

static char *build_source_debug_script_path(void) {
  char exe_path[PATH_MAX];
  ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
  if (len < 0) {
    return NULL;
  }
  exe_path[len] = '\0';

  char *last_slash = strrchr(exe_path, '/');
  if (last_slash == NULL) {
    return NULL;
  }
  *last_slash = '\0';

  char *bin_slash = strrchr(exe_path, '/');
  if (bin_slash != NULL && strcmp(bin_slash + 1, "bin") == 0) {
    *bin_slash = '\0';
  }

  size_t total_len = strlen(exe_path) + strlen("/src/debug/source_debug.py") + 1;
  char *script_path = malloc(total_len);
  snprintf(script_path, total_len, "%s/src/debug/source_debug.py", exe_path);
  return script_path;
}

bool start_source_debugger(void) {
  reap_source_debugger_if_exited();
  if (source_debugger_pid > 0) {
    return true;
  }
  if (!ensure_source_debug_dir()) {
    return false;
  }

  activate_source_debug();
  write_source_debug_state();

  char *script_path = build_source_debug_script_path();
  char *debuginfo_path = build_debuginfo_path();
  if (script_path == NULL || debuginfo_path == NULL) {
    free(script_path);
    free(debuginfo_path);
    return false;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    free(script_path);
    free(debuginfo_path);
    return false;
  }

  if (child_pid == 0) {
#ifdef __linux__
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0) {
      _exit(EXIT_FAILURE);
    }
#endif
    execlp("python3", "python3", script_path, debuginfo_path,
           SOURCE_DEBUG_STATE_PATH, NULL);
    _exit(EXIT_FAILURE);
  }

  free(script_path);
  free(debuginfo_path);
  source_debugger_pid = child_pid;
  return true;
}

void stop_source_debugger(void) {
  reap_source_debugger_if_exited();
  if (source_debugger_pid <= 0) {
    return;
  }

  kill(source_debugger_pid, SIGTERM);
  waitpid(source_debugger_pid, NULL, 0);
  source_debugger_pid = -1;
}

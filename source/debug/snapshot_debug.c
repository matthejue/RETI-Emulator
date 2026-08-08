#include "../../include/snapshot_debug.h"
#include "../../include/core_debug.h"
#include "../../include/input_output.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include "../../include/source_debug.h"
#include "../../include/tui.h"
#include "../../include/utils.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static bool snapshot_available = false;
static pid_t snapshot_child_pid = -1;
static int snapshot_child_write_fd = -1;

static bool copy_file_contents(const char *src_path, const char *dest_path) {
  int src_fd = open(src_path, O_RDONLY);
  if (src_fd < 0) {
    return false;
  }

  int dest_fd = open(dest_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (dest_fd < 0) {
    close(src_fd);
    return false;
  }

  char buffer[4096];
  ssize_t bytes_read;
  while ((bytes_read = read(src_fd, buffer, sizeof(buffer))) > 0) {
    ssize_t offset = 0;
    while (offset < bytes_read) {
      ssize_t written =
          write(dest_fd, buffer + offset, (size_t)(bytes_read - offset));
      if (written < 0) {
        close(src_fd);
        close(dest_fd);
        return false;
      }
      offset += written;
    }
  }

  if (bytes_read < 0) {
    close(src_fd);
    close(dest_fd);
    return false;
  }

  if (fsync(dest_fd) != 0) {
    close(src_fd);
    close(dest_fd);
    return false;
  }

  close(src_fd);
  close(dest_fd);
  return true;
}

static void set_snapshot_available(bool available) {
  snapshot_available = available;
  set_tui_snapshot_available(available);
}

void cleanup_snapshot_debug(void) {
  if (snapshot_child_write_fd != -1) {
    close(snapshot_child_write_fd);
    snapshot_child_write_fd = -1;
  }

  if (snapshot_child_pid > 0) {
    kill(snapshot_child_pid, SIGKILL);
    waitpid(snapshot_child_pid, NULL, 0);
    snapshot_child_pid = -1;
  }

  set_snapshot_available(false);
}

static bool copy_current_sram_to_snapshot(void) {
  char *sram_path = build_reti_emulator_file_path(peripherals_dir, "sram.bin");
  char *snapshot_path =
      build_reti_emulator_file_path(peripherals_dir, "snapshot_sram.bin");
  bool ok = fflush(sram) == 0 && fsync(fileno(sram)) == 0 &&
            copy_file_contents(sram_path, snapshot_path);
  free(sram_path);
  free(snapshot_path);
  return ok;
}

static bool reopen_snapshot_sram(void) {
  char *snapshot_path =
      build_reti_emulator_file_path(peripherals_dir, "snapshot_sram.bin");
  fclose(sram);
  sram = fopen(snapshot_path, "r+b");
  free(snapshot_path);
  return sram != NULL;
}

static void wait_for_restore_command(int read_fd) {
  while (true) {
    char command;
    if (read(read_fd, &command, 1) != 1) {
      _exit(0);
    }
    if (command != 'R') {
      continue;
    }

    int next_pipe[2];
    if (pipe(next_pipe) != 0) {
      _exit(1);
    }

    pid_t next_snapshot_pid = fork();
    if (next_snapshot_pid < 0) {
      _exit(1);
    }

    if (next_snapshot_pid == 0) {
      close(next_pipe[1]);
      close(read_fd);
      wait_for_restore_command(next_pipe[0]);
      return;
    }

    close(read_fd);
    close(next_pipe[0]);
    snapshot_child_pid = next_snapshot_pid;
    snapshot_child_write_fd = next_pipe[1];
    set_snapshot_available(true);

    if (!reopen_snapshot_sram()) {
      _exit(1);
    }
    sync_source_debug_state();
    return;
  }
}

// returns -1 on error, 0 in the restored child, 1 in the current process
static int create_snapshot(void) {
  if (!ensure_reti_emulator_directory(peripherals_dir)) {
    return -1;
  }

  if (!copy_current_sram_to_snapshot()) {
    return -1;
  }

  cleanup_snapshot_debug();

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    return -1;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    return -1;
  }

  if (child_pid == 0) {
    close(pipefd[1]);
    snapshot_child_pid = -1;
    snapshot_child_write_fd = -1;
    set_snapshot_available(false);
    wait_for_restore_command(pipefd[0]);
    return 0;
  }

  close(pipefd[0]);
  snapshot_child_pid = child_pid;
  snapshot_child_write_fd = pipefd[1];
  set_snapshot_available(true);
  return 1;
}

static bool restore_snapshot(pid_t *restored_pid) {
  if (snapshot_child_pid <= 0 || snapshot_child_write_fd == -1) {
    return false;
  }

  *restored_pid = snapshot_child_pid;
  if (write(snapshot_child_write_fd, "R", 1) != 1) {
    return false;
  }

  close(snapshot_child_write_fd);
  snapshot_child_write_fd = -1;
  snapshot_child_pid = -1;
  set_snapshot_available(false);
  return true;
}

bool handle_snapshot_debug_key(char key) {
  if (key == 'S') {
    int snapshot_result = create_snapshot();
    if (snapshot_result == 1) {
      display_notification_box("Snapshot",
                               "Saved process state to .reti_emulaor");
    } else if (snapshot_result == 0) {
      draw_tui();
      return true;
    } else {
      display_notification_box("Snapshot Error", "Snapshot failed");
    }
    draw_tui();
    return true;
  }

  if (key != 'R') {
    return false;
  }

  pid_t restored_pid;
  if (!snapshot_available) {
    display_notification_box("Restore Error", "No snapshot available yet");
    draw_tui();
    return true;
  }
  if (!restore_snapshot(&restored_pid)) {
    display_notification_box("Restore Error", "Restore failed");
    draw_tui();
    return true;
  }
  waitpid(restored_pid, NULL, 0);
  _exit(EXIT_SUCCESS);
}

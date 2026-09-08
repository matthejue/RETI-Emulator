#include "../include/guest_filesystem.h"
#include "../include/parse/parse_args.h"
#include "../include/uart.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static void send_byte(uint8_t byte) {
  uart[0] = byte;
  uart[2] &= 0b11111110;
  update_uart();
}

static void command(const char *text) {
  free(uart_input);
  uart_input = NULL;
  input_len = input_idx = 0;
  send_byte(27);
  while (*text != '\0') {
    send_byte(*text++);
  }
  send_byte(27);
  send_byte('/');
}

static uint32_t response(void) {
  assert(input_len >= 4);
  return (uint32_t)uart_input[0] << 24 | (uint32_t)uart_input[1] << 16 |
         (uint32_t)uart_input[2] << 8 | uart_input[3];
}

static void expect_status(const char *text, uint32_t status) {
  command(text);
  assert(input_len == 4);
  assert(response() == status);
}

static void write_host_file(const char *path, const char *content) {
  FILE *file = fopen(path, "wb");
  assert(file != NULL);
  assert(fwrite(content, 1, strlen(content), file) == strlen(content));
  assert(fclose(file) == 0);
}

static void expect_host_file(const char *path, const char *content) {
  FILE *file = fopen(path, "rb");
  assert(file != NULL);
  char buffer[64] = {0};
  assert(fread(buffer, 1, sizeof(buffer), file) == strlen(content));
  assert(strcmp(buffer, content) == 0);
  fclose(file);
}

static void test_regular_operations(void) {
  expect_status("mkdir /data", 0);
  expect_status("is-directory /data", 0);
  expect_status("touch /data/file", 0);
  command("write /data/file");
  send_byte('A');
  send_byte('B');
  send_byte('C');
  send_byte('D');
  command("write stdout");
  expect_host_file("data/file", "ABCD");
  expect_status("file-size /data/file", 4);
  command("load /data/file");
  assert(response() == 1 && input_len == 8);
  assert(memcmp(uart_input + 4, "ABCD", 4) == 0);
  command("read-range 1 2 /data/file");
  assert(response() == 2 && input_len == 6);
  assert(memcmp(uart_input + 4, "BC", 2) == 0);
  command("write-at 1 /data/file");
  send_byte('X');
  command("write stdout");
  expect_host_file("data/file", "AXCD");
  expect_status("move /data/file\n/data/renamed", 0);
  expect_status("file-size data//./renamed", 4);
  expect_status("touch /data/renamed", 0);
  expect_host_file("data/renamed", "AXCD");
  command("ls /data");
  assert(strstr((char *)uart_input + 4, "- renamed\n") != NULL);
  expect_status("unlink /data/renamed", 0);
  expect_status("rmdir /data", 0);
}

static void test_runtime_tmp(void) {
  command("pwd");
  assert(response() == 1 && input_len == 5 && uart_input[4] == '/');
  command("ls /");
  assert(strstr((char *)uart_input + 4, "d tmp\n") == NULL);
  expect_status("is-directory /tmp", UINT32_MAX);
  expect_status("touch /tmp/local", UINT32_MAX);
  expect_status("rmdir /", UINT32_MAX);
  expect_status("touch /../../root-file", 0);
  expect_status("file-size ../root-file", 0);
  assert(unlink("root-file") == 0);

  char temporary[] = "/tmp/reti-guest-host-XXXXXX";
  int fd = mkstemp(temporary);
  assert(fd >= 0);
  close(fd);
  write_host_file(temporary, "HOST");
  char text[512];
  snprintf(text, sizeof(text), "file-size %s", temporary);
  expect_status(text, UINT32_MAX);
  snprintf(text, sizeof(text), "unlink %s", temporary);
  expect_status(text, UINT32_MAX);

  // Uses an ordinary guest directory named tmp without exposing host /tmp
  expect_status("mkdir /tmp", 0);
  command("ls /");
  char *entry = strstr((char *)uart_input + 4, "d tmp\n");
  assert(entry != NULL && strstr(entry + 1, "d tmp\n") == NULL);
  expect_status("is-directory /tmp", 0);
  snprintf(text, sizeof(text), "write %s", temporary);
  command(text);
  send_byte('G');
  command("write stdout");
  expect_host_file(temporary + 1, "G");
  expect_host_file(temporary, "HOST");
  snprintf(text, sizeof(text), "unlink %s", temporary);
  expect_status(text, 0);
  expect_host_file(temporary, "HOST");
  expect_status("move /tmp\n/renamed-tmp", 0);
  expect_status("rmdir /renamed-tmp", 0);
  command("ls /");
  assert(strstr((char *)uart_input + 4, "d tmp\n") == NULL);
  assert(unlink(temporary) == 0);
}

static void test_escape_attempts(const char *outside) {
  write_host_file(outside, "KEEP");
  assert(symlink(outside, "escape") == 0);
  assert(symlink("/", "host") == 0);
  assert(symlink("/missing/host/target", "dangling") == 0);
  assert(link(outside, "hardlink") == 0);
  assert(mkfifo("fifo", 0600) == 0);
  const char *blocked[] = {"escape",
                           "dangling",
                           "hardlink",
                           "fifo",
                           "host/etc/passwd",
                           "/etc/passwd",
                           "C:/Windows/win.ini",
                           "//host/etc/passwd",
                           "host\\etc\\passwd",
                           "../outside"};
  char text[512];
  for (size_t i = 0; i < sizeof(blocked) / sizeof(*blocked); i++) {
    const char *operations[] = {"load ", "read-range 0 4 ", "file-size ",
                                "touch "};
    for (size_t j = 0; j < sizeof(operations) / sizeof(*operations); j++) {
      // A missing ordinary name may be created inside the root
      if (i == 9 && j == 3) {
        continue;
      }
      snprintf(text, sizeof(text), "%s%s", operations[j], blocked[i]);
      expect_status(text, UINT32_MAX);
    }
  }
  expect_status("mkdir /host/new-directory", UINT32_MAX);
  expect_status("is-directory /host", UINT32_MAX);
  expect_status("ls /host", UINT32_MAX);
  expect_status("unlink /host/etc/passwd", UINT32_MAX);
  expect_status("rmdir /host/etc", UINT32_MAX);
  expect_status("touch /local", 0);
  expect_status("move /local\n/host/new-file", UINT32_MAX);
  expect_status("move /host/etc/passwd\n/local", UINT32_MAX);
  expect_status("move /local\n/", UINT32_MAX);
  snprintf(text, sizeof(text), "file-size %s/../root/escape", outside);
  expect_status(text, UINT32_MAX);
  command("write /local");
  send_byte('L');
  command("write /escape");
  send_byte('X');
  command("write-at 0 /escape");
  send_byte('Y');
  command("write stdout");
  expect_host_file(outside, "KEEP");
  expect_host_file("local", "L");
  assert(unlink("escape") == 0);
  assert(unlink("host") == 0);
  assert(unlink("dangling") == 0);
  assert(unlink("hardlink") == 0);
  assert(unlink("fifo") == 0);
  assert(unlink("local") == 0);
}

static void test_symlink_replacement(const char *outside) {
  write_host_file("race-file", "SAFE");
  assert(symlink(outside, "race-spare") == 0);
  pid_t child = fork();
  assert(child >= 0);
  if (child == 0) {
    for (int i = 0; i < 1000; i++) {
      assert(rename("race-file", "race-moving") == 0);
      assert(rename("race-spare", "race-file") == 0);
      assert(rename("race-moving", "race-spare") == 0);
    }
    _exit(0);
  }
  for (int i = 0; i < 1000; i++) {
    FILE *file = guest_fopen("race-file", "r+b");
    if (file != NULL) {
      assert(fwrite("SAFE", 1, 4, file) == 4);
      fclose(file);
    }
  }
  int status;
  assert(waitpid(child, &status, 0) == child);
  assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
  expect_host_file(outside, "KEEP");
  assert(unlink("race-file") == 0);
  assert(unlink("race-spare") == 0);
}

int main(void) {
  int previous = open(".", O_RDONLY | O_DIRECTORY);
  char parent[] = "/tmp/reti-guest-test-XXXXXX";
  assert(mkdtemp(parent) != NULL);
  char root[512], outside[512];
  snprintf(root, sizeof(root), "%s/root", parent);
  snprintf(outside, sizeof(outside), "%s/outside", parent);
  assert(mkdir(root, 0700) == 0);
  assert(chdir(root) == 0);
  init_uart();
  max_waiting_instrs = 0;
  test_regular_operations();
  test_runtime_tmp();
  test_escape_attempts(outside);
  test_symlink_replacement(outside);
  // Keeps the original root even when the host changes its working directory
  assert(chdir(parent) == 0);
  expect_status("file-size /outside", UINT32_MAX);
  expect_status("touch /pinned", 0);
  assert(access("pinned", F_OK) != 0);
  assert(unlink("root/pinned") == 0);
  close_uart_output();
  close_guest_filesystem();
  free(uart_input);
  free(uart);
  assert(unlink(outside) == 0);
  assert(rmdir(root) == 0);
  assert(fchdir(previous) == 0);
  close(previous);
  assert(rmdir(parent) == 0);
  return 0;
}

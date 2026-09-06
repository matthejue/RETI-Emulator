#include "../../include/uart.h"
#include "../../include/dma.h"
#include "../../include/interrupt.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/interrupt_controller.h"
#include "../../include/input_output.h"
#include "../../include/special_opts.h"
#include "../../include/terminal_view.h"
#include "../../include/uart_terminal.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef _WIN32
#include <direct.h>
#include <io.h>
#include <sys/utime.h>
#define host_getcwd _getcwd
#define host_rmdir _rmdir
#define host_unlink _unlink
#define host_utime _utime
#else
#include <utime.h>
#define host_getcwd getcwd
#define host_rmdir rmdir
#define host_unlink unlink
#define host_utime utime
#endif

uint8_t *uart_input = NULL;
size_t input_len = 0;
size_t input_idx = 0;

uint8_t receive_current_byte = '\0';

uint8_t sending_waiting_time = 0;
uint8_t receiving_waiting_time = 0;

typedef enum { UART_IDLE, UART_WAITING } Uart_State;

static Uart_State send_state = UART_IDLE;
static Uart_State receive_state = UART_IDLE;

char *all_send_data = NULL;
char *current_send_data = NULL;
size_t all_send_data_len = 0;
size_t current_send_data_len = 0;

uint8_t *uart;

#define UART_SEND_READY 0b00000001
#define UART_RECEIVE_READY 0b00000010
#define UART_INPUT_BOX_LEN 80
#define UART_LOAD_COMMAND_PREFIX "load "
#define UART_READ_RANGE_COMMAND_PREFIX "read-range "
#define UART_FILE_SIZE_COMMAND_PREFIX "file-size "
#define UART_PWD_COMMAND "pwd"
#define UART_IS_DIRECTORY_COMMAND_PREFIX "is-directory "
#define UART_MKDIR_COMMAND_PREFIX "mkdir "
#define UART_LS_COMMAND "ls"
#define UART_LS_COMMAND_PREFIX "ls "
#define UART_UNLINK_COMMAND_PREFIX "unlink "
#define UART_RMDIR_COMMAND_PREFIX "rmdir "
#define UART_MOVE_COMMAND_PREFIX "move "
#define UART_TOUCH_COMMAND_PREFIX "touch "
#define UART_WRITE_COMMAND_PREFIX "write "
#define UART_WRITE_AT_COMMAND_PREFIX "write-at "
#define UART_CONTROL_ESCAPE 27
#define UART_CONTROL_MAX 4096

typedef enum {
  UART_OUTPUT_STDOUT,
  UART_OUTPUT_STDERR,
  UART_OUTPUT_FILE
} Uart_Output;

static char uart_control[UART_CONTROL_MAX + 1];
static size_t uart_control_len = 0;
static bool uart_control_active = false;
static bool uart_control_end_candidate = false;
static bool uart_control_overflow = false;
static Uart_Output uart_output = UART_OUTPUT_STDOUT;
static FILE *uart_output_file = NULL;

static bool is_visible_terminal_ascii(uint8_t byte) {
  return byte >= 32 && byte <= 126;
}

const char *format_uart_byte(uint8_t byte, char *buffer) {
  switch (byte) {
  case '\n':
    return "\\n";
  case '\t':
    return "\\t";
  case '\r':
    return "\\r";
  case '\0':
    return "\\0";
  case '\a':
    return "\\a";
  case '\b':
    return "\\b";
  case '\f':
    return "\\f";
  case '\v':
    return "\\v";
  case '\\':
    return "\\\\";
  }

  if (is_visible_terminal_ascii(byte)) {
    buffer[0] = byte;
    buffer[1] = '\0';
  } else {
    snprintf(buffer, 6, "\\d%03u", byte);
  }

  return buffer;
}

void init_uart() {
  close_uart_output();
  uart = malloc(sizeof(uint8_t) * NUM_PERIPHERY_ADDRESSES);
  memset(uart, 0, sizeof(uint8_t) * NUM_PERIPHERY_ADDRESSES);
  uart[2] = UART_SEND_READY | UART_RECEIVE_READY;
  write_array(uart, SYSTEM_INFO_TIMER_INTERRUPT_INTERVAL,
              interrupt_timer_interval, true);
  init_interrupt_controller();
}

static bool start_waiting(uint8_t *waiting_time) {
  if (max_waiting_instrs == 0) {
    *waiting_time = 0;
    return false;
  }

  *waiting_time = rand() % max_waiting_instrs + 1;
  return true;
}

static bool waiting_finished(uint8_t *waiting_time) {
  (*waiting_time)--;
  return *waiting_time == 0;
}

static bool is_digit(uint8_t ch) {
  return ch >= '0' && ch <= '9';
}

static void append_newline(uint8_t *input, size_t *len) {
  input[(*len)++] = '\n';
  input[*len] = '\0';
}

static void append_byte_to_buffer(char **buffer, size_t *len, uint8_t byte) {
  *buffer = realloc(*buffer, *len + 1);
  (*buffer)[(*len)++] = byte;
}

static void remember_sent_byte(uint8_t sent_byte) {
  if (!debug_mode) {
    return;
  }

  free(current_send_data);
  current_send_data = NULL;
  current_send_data_len = 0;
  append_byte_to_buffer(&current_send_data, &current_send_data_len, sent_byte);
  append_byte_to_buffer(&all_send_data, &all_send_data_len, sent_byte);
}

static void append_uart_input_bytes(const uint8_t *input, size_t len) {
  if (len == 0) {
    return;
  }
  if (SIZE_MAX - input_len <= len) {
    fprintf(stderr, "Error: UART input buffer too large\n");
    exit(EXIT_FAILURE);
  }

  uint8_t *new_input = realloc(uart_input, input_len + len + 1);
  if (new_input == NULL) {
    fprintf(stderr, "Error: Couldn't allocate UART input buffer\n");
    exit(EXIT_FAILURE);
  }

  memcpy(new_input + input_len, input, len);
  input_len += len;
  new_input[input_len] = '\0';
  uart_input = new_input;
}

static void append_uart_input_u32(uint32_t value) {
  uint8_t bytes[sizeof(uint32_t)] = {
      (uint8_t)(value >> 24),
      (uint8_t)(value >> 16),
      (uint8_t)(value >> 8),
      (uint8_t)value,
  };
  append_uart_input_bytes(bytes, sizeof(bytes));
}

static void append_uart_input_string(const char *value) {
  size_t length = strlen(value);
  if (length >= UINT32_MAX) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  append_uart_input_u32((uint32_t)length);
  append_uart_input_bytes((const uint8_t *)value, length);
}

static char *trim_uart_path(char *path) {
  while (*path == ' ' || *path == '\t') {
    path++;
  }
  char *end = path + strlen(path);
  while (end > path && (end[-1] == ' ' || end[-1] == '\t')) {
    *--end = '\0';
  }
  return path;
}

static bool load_file_into_uart_input(const char *path, size_t len) {
  if (len / sizeof(uint32_t) >= UINT32_MAX) {
    fprintf(stderr, "Warning: UART input file %s contains too many words\n",
            path);
    return false;
  }

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Warning: Couldn't load UART input file %s: %s\n", path,
            strerror(errno));
    return false;
  }

  uint8_t *buffer = malloc(len == 0 ? 1 : len);
  if (buffer == NULL) {
    fclose(file);
    fprintf(stderr, "Error: Couldn't allocate UART input file buffer\n");
    exit(EXIT_FAILURE);
  }

  size_t bytes_read = fread(buffer, 1, len, file);
  if (bytes_read != len) {
    fprintf(stderr, "Warning: Couldn't read complete UART input file %s\n",
            path);
  }
  fclose(file);

  append_uart_input_u32((uint32_t)(bytes_read / sizeof(uint32_t)));
  append_uart_input_bytes(buffer, bytes_read);
  free(buffer);
  return true;
}

static void append_uart_read_range_error(void) {
  append_uart_input_u32(UINT32_MAX);
}

static void process_uart_read_range_command(char *command) {
  char *cursor = command + strlen(UART_READ_RANGE_COMMAND_PREFIX);
  char *number_end;
  errno = 0;
  unsigned long offset = strtoul(cursor, &number_end, 10);
  if (errno != 0 || number_end == cursor ||
      (*number_end != ' ' && *number_end != '\t') ||
      offset > UINT32_MAX) {
    append_uart_read_range_error();
    return;
  }

  cursor = number_end;
  while (*cursor == ' ' || *cursor == '\t') {
    cursor++;
  }
  errno = 0;
  unsigned long count = strtoul(cursor, &number_end, 10);
  if (errno != 0 || number_end == cursor ||
      (*number_end != ' ' && *number_end != '\t') ||
      count > UINT32_MAX) {
    append_uart_read_range_error();
    return;
  }

  cursor = number_end;
  while (*cursor == ' ' || *cursor == '\t') {
    cursor++;
  }
  char *path = cursor;
  char *path_end = path + strlen(path);
  while (path_end > path &&
         (path_end[-1] == ' ' || path_end[-1] == '\t')) {
    *--path_end = '\0';
  }

  struct stat st;
  if (*path == '\0' || stat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_size < 0 || (uintmax_t)st.st_size >= UINT32_MAX) {
    append_uart_read_range_error();
    return;
  }

  uint32_t file_size = (uint32_t)st.st_size;
  size_t available = offset < file_size ? file_size - offset : 0;
  size_t requested = count < available ? count : available;
  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    append_uart_read_range_error();
    return;
  }
  if (requested == 0) {
    fclose(file);
    append_uart_input_u32(0);
    return;
  }

  if (fseek(file, (long)offset, SEEK_SET) != 0) {
    fclose(file);
    append_uart_read_range_error();
    return;
  }

  uint8_t *buffer = malloc(requested);
  if (buffer == NULL) {
    fclose(file);
    fprintf(stderr, "Error: Couldn't allocate UART input file buffer\n");
    exit(EXIT_FAILURE);
  }
  size_t bytes_read = fread(buffer, 1, requested, file);
  fclose(file);

  append_uart_input_u32((uint32_t)bytes_read);
  append_uart_input_bytes(buffer, bytes_read);
  free(buffer);
}

static void process_uart_file_size_command(char *command) {
  char *path = command + strlen(UART_FILE_SIZE_COMMAND_PREFIX);
  while (*path == ' ' || *path == '\t') {
    path++;
  }

  char *path_end = path + strlen(path);
  while (path_end > path &&
         (path_end[-1] == ' ' || path_end[-1] == '\t')) {
    *--path_end = '\0';
  }

  struct stat st;
  if (*path == '\0' || stat(path, &st) != 0 || !S_ISREG(st.st_mode) ||
      st.st_size < 0 || (uintmax_t)st.st_size >= UINT32_MAX) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }

  FILE *file = fopen(path, "rb");
  if (file == NULL) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  fclose(file);
  append_uart_input_u32((uint32_t)st.st_size);
}

static void process_uart_pwd_command(void) {
  char path[UART_CONTROL_MAX + 1];

  if (host_getcwd(path, sizeof(path)) == NULL) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  append_uart_input_string(path);
}

static void process_uart_is_directory_command(char *command) {
  char *path =
      trim_uart_path(command + strlen(UART_IS_DIRECTORY_COMMAND_PREFIX));
  struct stat st;

  append_uart_input_u32(*path != '\0' && stat(path, &st) == 0 &&
                                S_ISDIR(st.st_mode)
                            ? 0
                            : UINT32_MAX);
}

static int make_directory(const char *path) {
#ifdef _WIN32
  return _mkdir(path);
#else
  return mkdir(path, 0777);
#endif
}

static void process_uart_mkdir_command(char *command) {
  char *path = trim_uart_path(command + strlen(UART_MKDIR_COMMAND_PREFIX));
  append_uart_input_u32(*path != '\0' && make_directory(path) == 0 ? 0
                                                                   : UINT32_MAX);
}

static bool append_directory_output(char **output, size_t *length,
                                    const char *text) {
  size_t text_length = strlen(text);
  if (SIZE_MAX - *length <= text_length) {
    return false;
  }
  char *new_output = realloc(*output, *length + text_length + 1);
  if (new_output == NULL) {
    return false;
  }
  memcpy(new_output + *length, text, text_length + 1);
  *output = new_output;
  *length += text_length;
  return true;
}

static int compare_directory_entries(const void *left, const void *right) {
  const char *left_entry = *(const char *const *)left;
  const char *right_entry = *(const char *const *)right;
  return strcmp(left_entry + 2, right_entry + 2);
}

static void process_uart_ls_command(char *command) {
  char *path = trim_uart_path(command + strlen(UART_LS_COMMAND_PREFIX));
  if (*path == '\0') {
    append_uart_input_u32(UINT32_MAX);
    return;
  }

  DIR *directory = opendir(path);
  if (directory == NULL) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  char **entries = NULL;
  size_t entry_count = 0;
  bool success = true;
  struct dirent *entry;
  while ((entry = readdir(directory)) != NULL && success) {
    size_t entry_length = strlen(entry->d_name) + 4;
    char *formatted_entry = malloc(entry_length);
    char **new_entries;

    if (formatted_entry == NULL) {
      success = false;
      break;
    }
    snprintf(formatted_entry, entry_length, "%s%s\n",
             entry->d_type == DT_DIR ? "d " : "- ", entry->d_name);
    new_entries = realloc(entries, (entry_count + 1) * sizeof(*entries));
    if (new_entries == NULL) {
      free(formatted_entry);
      success = false;
      break;
    }
    entries = new_entries;
    entries[entry_count] = formatted_entry;
    entry_count++;
  }
  closedir(directory);

  if (entry_count > 1) {
    qsort(entries, entry_count, sizeof(*entries), compare_directory_entries);
  }
  char *output = NULL;
  size_t output_length = 0;
  for (size_t index = 0; index < entry_count; index++) {
    if (success) {
      success = append_directory_output(&output, &output_length, entries[index]);
    }
    free(entries[index]);
  }
  free(entries);
  if (!success || output_length >= UINT32_MAX) {
    free(output);
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  append_uart_input_u32((uint32_t)output_length);
  append_uart_input_bytes((uint8_t *)output, output_length);
  free(output);
}

static void process_uart_unlink_command(char *command) {
  char *path = trim_uart_path(command + strlen(UART_UNLINK_COMMAND_PREFIX));
  append_uart_input_u32(*path != '\0' && host_unlink(path) == 0 ? 0
                                                                : UINT32_MAX);
}

static void process_uart_rmdir_command(char *command) {
  char *path = trim_uart_path(command + strlen(UART_RMDIR_COMMAND_PREFIX));
  append_uart_input_u32(*path != '\0' && host_rmdir(path) == 0 ? 0
                                                               : UINT32_MAX);
}

static void process_uart_move_command(char *command) {
  char *old_path = command + strlen(UART_MOVE_COMMAND_PREFIX);
  char *new_path = strchr(old_path, '\n');

  if (new_path == NULL) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  *new_path = '\0';
  new_path++;
  append_uart_input_u32(*old_path != '\0' && *new_path != '\0' &&
                                rename(old_path, new_path) == 0
                            ? 0
                            : UINT32_MAX);
}

static void process_uart_touch_command(char *command) {
  char *path = trim_uart_path(command + strlen(UART_TOUCH_COMMAND_PREFIX));
  FILE *file = fopen(path, "ab");

  if (file == NULL) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  fclose(file);
  append_uart_input_u32(host_utime(path, NULL) == 0 ? 0 : UINT32_MAX);
}

static void process_uart_load_command(char *command) {
  const size_t prefix_len = strlen(UART_LOAD_COMMAND_PREFIX);
  char *path = command + prefix_len;
  while (*path == ' ' || *path == '\t') {
    path++;
  }

  char *end = path + strlen(path);
  while (end > path && (end[-1] == ' ' || end[-1] == '\t')) {
    *--end = '\0';
  }
  if (*path == '\0') {
    append_uart_input_u32(UINT32_MAX);
    return;
  }

  struct stat st;
  if (stat(path, &st) != 0) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  if (S_ISDIR(st.st_mode)) {
    fprintf(stderr,
            "Warning: UART load directory chooser is not implemented; send "
            "<esc>load <file><esc>/ instead\n");
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  if (!S_ISREG(st.st_mode)) {
    append_uart_input_u32(UINT32_MAX);
    return;
  }
  if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX) {
    fprintf(stderr, "Warning: UART input file %s is too large to load\n", path);
    append_uart_input_u32(UINT32_MAX);
    return;
  }

  if (!load_file_into_uart_input(path, (size_t)(uintmax_t)st.st_size)) {
    append_uart_input_u32(UINT32_MAX);
  }
}

static void activate_uart_output_file(FILE *file) {
  if (uart_output_file != NULL) {
    fclose(uart_output_file);
  }
  uart_output_file = file;
  uart_output = UART_OUTPUT_FILE;
}

static void select_uart_output_file(const char *path, const char *mode) {
  FILE *file = fopen(path, mode);
  if (file == NULL) {
    fprintf(stderr, "Warning: Couldn't open UART output file %s: %s\n", path,
            strerror(errno));
    return;
  }
  activate_uart_output_file(file);
}

static void select_uart_output_file_at(const char *path, long offset) {
  FILE *file = fopen(path, "r+b");
  if (file == NULL && errno == ENOENT) {
    file = fopen(path, "w+b");
  }
  if (file == NULL) {
    fprintf(stderr, "Warning: Couldn't open UART output file %s: %s\n", path,
            strerror(errno));
    return;
  }
  if (fseek(file, offset, SEEK_SET) != 0) {
    fprintf(stderr, "Warning: Couldn't seek UART output file %s: %s\n", path,
            strerror(errno));
    fclose(file);
    return;
  }

  activate_uart_output_file(file);
}

static void select_uart_standard_output(Uart_Output output) {
  if (uart_output_file != NULL) {
    fclose(uart_output_file);
    uart_output_file = NULL;
  }
  uart_output = output;
}

static void process_uart_write_command(char *command) {
  char *destination = command + strlen(UART_WRITE_COMMAND_PREFIX);

  if (strcmp(destination, "stdout") == 0) {
    select_uart_standard_output(UART_OUTPUT_STDOUT);
  } else if (strcmp(destination, "stderr") == 0) {
    select_uart_standard_output(UART_OUTPUT_STDERR);
  } else if (*destination != '\0') {
    select_uart_output_file(destination, "wb");
  }
}

static void process_uart_write_at_command(char *command) {
  char *cursor = command + strlen(UART_WRITE_AT_COMMAND_PREFIX);
  char *number_end;
  errno = 0;
  unsigned long offset = strtoul(cursor, &number_end, 10);
  if (errno != 0 || number_end == cursor ||
      (*number_end != ' ' && *number_end != '\t') || offset > LONG_MAX) {
    return;
  }

  char *path = trim_uart_path(number_end);
  if (*path != '\0') {
    select_uart_output_file_at(path, (long)offset);
  }
}

static void process_uart_control(void) {
  uart_control[uart_control_len] = '\0';
  if (strncmp(uart_control, UART_LOAD_COMMAND_PREFIX,
              strlen(UART_LOAD_COMMAND_PREFIX)) == 0) {
    process_uart_load_command(uart_control);
  } else if (strncmp(uart_control, UART_READ_RANGE_COMMAND_PREFIX,
                     strlen(UART_READ_RANGE_COMMAND_PREFIX)) == 0) {
    process_uart_read_range_command(uart_control);
  } else if (strncmp(uart_control, UART_FILE_SIZE_COMMAND_PREFIX,
                     strlen(UART_FILE_SIZE_COMMAND_PREFIX)) == 0) {
    process_uart_file_size_command(uart_control);
  } else if (strcmp(uart_control, UART_PWD_COMMAND) == 0) {
    process_uart_pwd_command();
  } else if (strncmp(uart_control, UART_IS_DIRECTORY_COMMAND_PREFIX,
                     strlen(UART_IS_DIRECTORY_COMMAND_PREFIX)) == 0) {
    process_uart_is_directory_command(uart_control);
  } else if (strncmp(uart_control, UART_MKDIR_COMMAND_PREFIX,
                     strlen(UART_MKDIR_COMMAND_PREFIX)) == 0) {
    process_uart_mkdir_command(uart_control);
  } else if (strcmp(uart_control, UART_LS_COMMAND) == 0) {
    char default_ls_command[] = "ls .";
    process_uart_ls_command(default_ls_command);
  } else if (strncmp(uart_control, UART_LS_COMMAND_PREFIX,
                     strlen(UART_LS_COMMAND_PREFIX)) == 0) {
    process_uart_ls_command(uart_control);
  } else if (strncmp(uart_control, UART_UNLINK_COMMAND_PREFIX,
                     strlen(UART_UNLINK_COMMAND_PREFIX)) == 0) {
    process_uart_unlink_command(uart_control);
  } else if (strncmp(uart_control, UART_RMDIR_COMMAND_PREFIX,
                     strlen(UART_RMDIR_COMMAND_PREFIX)) == 0) {
    process_uart_rmdir_command(uart_control);
  } else if (strncmp(uart_control, UART_MOVE_COMMAND_PREFIX,
                     strlen(UART_MOVE_COMMAND_PREFIX)) == 0) {
    process_uart_move_command(uart_control);
  } else if (strncmp(uart_control, UART_TOUCH_COMMAND_PREFIX,
                     strlen(UART_TOUCH_COMMAND_PREFIX)) == 0) {
    process_uart_touch_command(uart_control);
  } else if (strncmp(uart_control, UART_WRITE_AT_COMMAND_PREFIX,
                     strlen(UART_WRITE_AT_COMMAND_PREFIX)) == 0) {
    process_uart_write_at_command(uart_control);
  } else if (strncmp(uart_control, UART_WRITE_COMMAND_PREFIX,
                     strlen(UART_WRITE_COMMAND_PREFIX)) == 0) {
    process_uart_write_command(uart_control);
  }
}

static void append_uart_control_byte(uint8_t byte) {
  if (uart_control_overflow) {
    return;
  }

  if (uart_control_len >= UART_CONTROL_MAX) {
    uart_control_overflow = true;
    return;
  }
  uart_control[uart_control_len++] = byte;
}

static bool handle_uart_control_byte(uint8_t byte) {
  if (!uart_control_active) {
    if (byte != UART_CONTROL_ESCAPE) {
      return false;
    }
    uart_control_active = true;
    uart_control_len = 0;
    uart_control_end_candidate = false;
    uart_control_overflow = false;
    return true;
  }

  if (uart_control_end_candidate) {
    if (byte == '/') {
      if (!uart_control_overflow) {
        process_uart_control();
      }
      uart_control_active = false;
      uart_control_end_candidate = false;
      return true;
    }

    append_uart_control_byte(UART_CONTROL_ESCAPE);
    uart_control_end_candidate = false;
  }

  if (byte == UART_CONTROL_ESCAPE) {
    uart_control_end_candidate = true;
  } else {
    append_uart_control_byte(byte);
  }
  return true;
}

static void write_uart_stdout(uint8_t byte) {
  if (debug_mode) {
    append_terminal_output(byte);
    if (!uart_terminal_is_active()) {
      return;
    }
  }

  if (test_mode) {
    char buffer[6];
    adjust_print(true, "%s", "%s", format_uart_byte(byte, buffer));
  } else {
    adjust_print(true, "%c", "%c", byte);
    fflush(stdout);
  }
}

static void write_uart_output(uint8_t byte) {
  switch (uart_output) {
  case UART_OUTPUT_STDOUT:
    write_uart_stdout(byte);
    break;
  case UART_OUTPUT_STDERR:
    if (test_mode) {
      char buffer[6];
      adjust_print(false, "%s", "%s", format_uart_byte(byte, buffer));
    } else {
      fputc(byte, stderr);
      fflush(stderr);
    }
    break;
  case UART_OUTPUT_FILE:
    fputc(byte, uart_output_file);
    fflush(uart_output_file);
    break;
  }
}

void close_uart_output(void) {
  if (uart_output_file != NULL) {
    fclose(uart_output_file);
    uart_output_file = NULL;
  }
  uart_output = UART_OUTPUT_STDOUT;
  uart_control_active = false;
  uart_control_end_candidate = false;
  uart_control_overflow = false;
  uart_control_len = 0;
}

static bool uart_send_requested(void) {
  return !(read_array(uart, 2, true) & UART_SEND_READY);
}

static bool uart_receive_requested(void) {
  return !(read_array(uart, 2, true) & UART_RECEIVE_READY);
}

static bool uart_send_completes_input_control(void) {
  size_t load_prefix_len = strlen(UART_LOAD_COMMAND_PREFIX);
  size_t read_range_prefix_len = strlen(UART_READ_RANGE_COMMAND_PREFIX);
  size_t file_size_prefix_len = strlen(UART_FILE_SIZE_COMMAND_PREFIX);
  size_t is_directory_prefix_len =
      strlen(UART_IS_DIRECTORY_COMMAND_PREFIX);
  size_t mkdir_prefix_len = strlen(UART_MKDIR_COMMAND_PREFIX);
  size_t ls_prefix_len = strlen(UART_LS_COMMAND_PREFIX);
  size_t unlink_prefix_len = strlen(UART_UNLINK_COMMAND_PREFIX);
  size_t rmdir_prefix_len = strlen(UART_RMDIR_COMMAND_PREFIX);
  size_t move_prefix_len = strlen(UART_MOVE_COMMAND_PREFIX);
  size_t touch_prefix_len = strlen(UART_TOUCH_COMMAND_PREFIX);
  return uart_send_requested() && uart_control_active &&
         uart_control_end_candidate && uart[0] == '/' &&
         !uart_control_overflow &&
         ((uart_control_len >= load_prefix_len &&
           memcmp(uart_control, UART_LOAD_COMMAND_PREFIX, load_prefix_len) ==
               0) ||
          (uart_control_len >= read_range_prefix_len &&
           memcmp(uart_control, UART_READ_RANGE_COMMAND_PREFIX,
                  read_range_prefix_len) == 0) ||
          (uart_control_len >= file_size_prefix_len &&
           memcmp(uart_control, UART_FILE_SIZE_COMMAND_PREFIX,
                  file_size_prefix_len) == 0) ||
          (uart_control_len == strlen(UART_PWD_COMMAND) &&
           memcmp(uart_control, UART_PWD_COMMAND,
                  strlen(UART_PWD_COMMAND)) == 0) ||
          (uart_control_len >= is_directory_prefix_len &&
           memcmp(uart_control, UART_IS_DIRECTORY_COMMAND_PREFIX,
                  is_directory_prefix_len) == 0) ||
          (uart_control_len >= mkdir_prefix_len &&
           memcmp(uart_control, UART_MKDIR_COMMAND_PREFIX,
                  mkdir_prefix_len) == 0) ||
          (uart_control_len == strlen(UART_LS_COMMAND) &&
           memcmp(uart_control, UART_LS_COMMAND,
                  strlen(UART_LS_COMMAND)) == 0) ||
          (uart_control_len >= ls_prefix_len &&
           memcmp(uart_control, UART_LS_COMMAND_PREFIX, ls_prefix_len) == 0) ||
          (uart_control_len >= unlink_prefix_len &&
           memcmp(uart_control, UART_UNLINK_COMMAND_PREFIX,
                  unlink_prefix_len) == 0) ||
          (uart_control_len >= rmdir_prefix_len &&
           memcmp(uart_control, UART_RMDIR_COMMAND_PREFIX,
                  rmdir_prefix_len) == 0) ||
          (uart_control_len >= move_prefix_len &&
           memcmp(uart_control, UART_MOVE_COMMAND_PREFIX,
                  move_prefix_len) == 0) ||
          (uart_control_len >= touch_prefix_len &&
           memcmp(uart_control, UART_TOUCH_COMMAND_PREFIX,
                  touch_prefix_len) == 0));
}

static void complete_send(void) {
  uint8_t sent_byte = uart[0];
  if (!handle_uart_control_byte(sent_byte)) {
    write_uart_output(sent_byte);
  }
  remember_sent_byte(sent_byte);
  uart[2] = uart[2] | UART_SEND_READY;
  send_state = UART_IDLE;
}

static void update_uart_send(void) {
  switch (send_state) {
  case UART_IDLE:
    if (!uart_send_requested()) {
      return;
    }

    if (start_waiting(&sending_waiting_time)) {
      send_state = UART_WAITING;
    } else {
      complete_send();
    }
    break;
  case UART_WAITING:
    if (waiting_finished(&sending_waiting_time)) {
      complete_send();
    }
    break;
  }
}

static void set_uart_input_buffer(const uint8_t *input, size_t len) {
  free(uart_input);
  uart_input = malloc(len + 1);
  memcpy(uart_input, input, len);
  uart_input[len] = '\0';
  input_len = len;
  input_idx = 0;
}

uint16_t decode_uart_input_escapes(uint8_t *input, uint16_t len) {
  uint16_t read_idx = 0;
  uint16_t write_idx = 0;

  while (read_idx < len) {
    if (input[read_idx] == '\\' && read_idx + 1 < len) {
      switch (input[read_idx + 1]) {
      case 'n':
        input[write_idx++] = '\n';
        read_idx += 2;
        continue;
      case 't':
        input[write_idx++] = '\t';
        read_idx += 2;
        continue;
      case 'r':
        input[write_idx++] = '\r';
        read_idx += 2;
        continue;
      case '0':
        input[write_idx++] = '\0';
        read_idx += 2;
        continue;
      case 'a':
        input[write_idx++] = '\a';
        read_idx += 2;
        continue;
      case 'b':
        input[write_idx++] = '\b';
        read_idx += 2;
        continue;
      case 'f':
        input[write_idx++] = '\f';
        read_idx += 2;
        continue;
      case 'v':
        input[write_idx++] = '\v';
        read_idx += 2;
        continue;
      case '\\':
        input[write_idx++] = '\\';
        read_idx += 2;
        continue;
      }
    }

    if (input[read_idx] == '\\' && read_idx + 4 < len &&
        input[read_idx + 1] == 'd' && is_digit(input[read_idx + 2]) &&
        is_digit(input[read_idx + 3]) && is_digit(input[read_idx + 4])) {
      uint16_t value = (input[read_idx + 2] - '0') * 100 +
                       (input[read_idx + 3] - '0') * 10 +
                       (input[read_idx + 4] - '0');
      if (value <= UINT8_MAX) {
        input[write_idx++] = (uint8_t)value;
        read_idx += 5;
      } else {
        input[write_idx++] = input[read_idx++];
      }
      continue;
    }

    input[write_idx++] = input[read_idx++];
  }

  input[write_idx] = '\0';
  return write_idx;
}

static void normalize_uart_input(uint8_t *input, size_t *len) {
  if (input[0] == '\0') {
    input[0] = '\n';
    input[1] = '\0';
    *len = 1;
  } else {
    *len = decode_uart_input_escapes(input, strlen((char *)input));
    append_newline(input, len);
  }
}

static bool ask_for_uart_input(void) {
  uint8_t input[UART_INPUT_BOX_LEN + 2];
  size_t len;

  if (debug_mode) {
    if (!display_input_box((char *)input, "UART input (empty = newline):",
                           UART_INPUT_BOX_LEN)) {
      return false;
    }
  } else {
    fflush(stdout);
    printf("\nUART input (empty = newline): ");
    fflush(stdout);

    if (fgets((char *)input, UART_INPUT_BOX_LEN + 1, stdin) == NULL) {
      input[0] = '\0';
    } else {
      len = strlen((char *)input);
      if (len > 0 && input[len - 1] == '\n') {
        input[len - 1] = '\0';
      } else {
        int ch;
        while ((ch = getchar()) != '\n' && ch != EOF) {
        }
      }
    }
  }

  normalize_uart_input(input, &len);

  set_uart_input_buffer(input, len);
  return true;
}

static bool uart_receive_buffer_has_data(void) { return input_idx < input_len; }

static void complete_receive(void) {
  uart[1] = receive_current_byte;
  uart[2] = uart[2] | UART_RECEIVE_READY;
  receive_state = UART_IDLE;
}

static void update_uart_receive(void) {
  switch (receive_state) {
  case UART_IDLE:
    if (!uart_receive_requested()) {
      return;
    }
    if (uart_terminal_is_active() && !uart_receive_buffer_has_data()) {
      return;
    }
    if (!uart_receive_buffer_has_data() && !ask_for_uart_input()) {
      return;
    }
    receive_current_byte = uart_input[input_idx++];
    if (start_waiting(&receiving_waiting_time)) {
      receive_state = UART_WAITING;
    } else {
      complete_receive();
    }
    break;
  case UART_WAITING:
    if (waiting_finished(&receiving_waiting_time)) {
      complete_receive();
    }
    break;
  }
}

void update_uart(void) {
  if (uart_send_completes_input_control()) {
    update_uart_send();
    if (uart_send_requested()) {
      return;
    }
    update_uart_receive();
    return;
  }

  update_uart_receive();
  update_uart_send();
}

bool uart_dma_read_word(uint32_t *word) {
  if (input_len - input_idx < sizeof(uint32_t)) {
    return false;
  }

  *word = 0;
  for (size_t index = 0; index < sizeof(uint32_t); index++) {
    uint8_t byte = uart_input[input_idx++];
    uart[1] = byte;
    *word = (*word << 8) | byte;
  }
  return true;
}

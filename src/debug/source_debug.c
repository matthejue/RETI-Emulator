#include "../../include/source_debug.h"
#include "../../include/assemble.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
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

typedef struct {
  char *name;
  char *scope;
  uint32_t address;
  uint32_t size;
  bool is_argument;
} SourceDebugSymbol;

typedef struct {
  uint32_t address;
  char *target_function;
} SourceDebugCallJump;

typedef struct {
  const char *function_name;
  uint64_t baf;
} SourceDebugStackFrame;

static SourceDebugSymbol *source_debug_symbols = NULL;
static size_t num_source_debug_symbols = 0;
static size_t source_debug_symbols_capacity = 0;
static SourceDebugCallJump *source_debug_call_jumps = NULL;
static size_t num_source_debug_call_jumps = 0;
static size_t source_debug_call_jumps_capacity = 0;
static uint32_t *source_debug_return_addresses = NULL;
static size_t num_source_debug_return_addresses = 0;
static size_t source_debug_return_addresses_capacity = 0;
static SourceDebugStackFrame *source_debug_call_stack = NULL;
static size_t source_debug_call_stack_size = 0;
static size_t source_debug_call_stack_capacity = 0;
static bool source_debug_last_event_valid = false;
static uint32_t source_debug_last_event_pc = 0;
static uint32_t source_debug_last_event_cs = 0;
static bool source_debug_symbols_load_attempted = false;
const char *current_stackframe_function = NULL;

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
  const char *filename = last_slash == NULL ? sram_prgrm_path : last_slash + 1;
  const char *last_dot = strrchr(filename, '.');
  size_t basename_len =
      last_dot == NULL ? strlen(filename) : (size_t)(last_dot - filename);
  size_t dir_len = last_slash == NULL ? 0 : (size_t)(last_slash - sram_prgrm_path);
  size_t total_len = dir_len + (dir_len > 0 ? 1 : 0) + basename_len +
                     strlen(".debuginfo") + 1;
  char *path = malloc(total_len);
  if (path == NULL) {
    return NULL;
  }

  if (dir_len > 0) {
    strncpy(path, sram_prgrm_path, dir_len);
    path[dir_len] = '/';
    strncpy(path + dir_len + 1, filename, basename_len);
    path[dir_len + 1 + basename_len] = '\0';
  } else {
    strncpy(path, filename, basename_len);
    path[basename_len] = '\0';
  }
  strcat(path, ".debuginfo");
  return path;
}

static char *read_text_file_or_null(const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  long size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  char *content = malloc((size_t)size + 1);
  if (content == NULL) {
    fclose(file);
    return NULL;
  }

  size_t read_len = fread(content, 1, (size_t)size, file);
  content[read_len] = '\0';
  fclose(file);
  return content;
}

static char *skip_json_ws(char *cursor) {
  while (*cursor == ' ' || *cursor == '\n' || *cursor == '\r' ||
         *cursor == '\t') {
    cursor++;
  }
  return cursor;
}

static char *find_json_array(char *json, const char *key) {
  char search_key[64];
  snprintf(search_key, sizeof(search_key), "\"%s\"", key);

  char *cursor = strstr(json, search_key);
  if (cursor == NULL) {
    return NULL;
  }

  cursor = strchr(cursor, '[');
  return cursor == NULL ? NULL : cursor + 1;
}

static char *find_json_object_end(char *object_start) {
  bool in_string = false;
  bool escaped = false;
  int depth = 0;

  for (char *cursor = object_start; *cursor != '\0'; cursor++) {
    if (escaped) {
      escaped = false;
      continue;
    }
    if (*cursor == '\\' && in_string) {
      escaped = true;
      continue;
    }
    if (*cursor == '"') {
      in_string = !in_string;
      continue;
    }
    if (in_string) {
      continue;
    }
    if (*cursor == '{') {
      depth++;
    } else if (*cursor == '}') {
      depth--;
      if (depth == 0) {
        return cursor;
      }
    }
  }

  return NULL;
}

static char *json_string_field(char *object_start, char *object_end,
                               const char *key) {
  char search_key[64];
  snprintf(search_key, sizeof(search_key), "\"%s\"", key);

  char *cursor = object_start;
  while ((cursor = strstr(cursor, search_key)) != NULL && cursor < object_end) {
    cursor += strlen(search_key);
    cursor = skip_json_ws(cursor);
    if (*cursor != ':') {
      continue;
    }
    cursor = skip_json_ws(cursor + 1);
    if (*cursor != '"') {
      continue;
    }
    cursor++;
    char *value_start = cursor;
    while (cursor < object_end && *cursor != '\0' && *cursor != '"') {
      if (*cursor == '\\' && cursor[1] != '\0') {
        cursor += 2;
      } else {
        cursor++;
      }
    }
    if (cursor >= object_end || *cursor != '"') {
      return NULL;
    }
    size_t value_len = (size_t)(cursor - value_start);
    char *value = malloc(value_len + 1);
    if (value == NULL) {
      return NULL;
    }
    memcpy(value, value_start, value_len);
    value[value_len] = '\0';
    return value;
  }

  return NULL;
}

static bool json_u32_field(char *object_start, char *object_end,
                           const char *key, uint32_t *value) {
  char search_key[64];
  snprintf(search_key, sizeof(search_key), "\"%s\"", key);

  char *cursor = object_start;
  while ((cursor = strstr(cursor, search_key)) != NULL && cursor < object_end) {
    cursor += strlen(search_key);
    cursor = skip_json_ws(cursor);
    if (*cursor != ':') {
      continue;
    }
    cursor = skip_json_ws(cursor + 1);
    char *endptr = NULL;
    unsigned long parsed = strtoul(cursor, &endptr, 10);
    if (endptr == cursor || endptr > object_end || parsed > UINT32_MAX) {
      return false;
    }
    *value = (uint32_t)parsed;
    return true;
  }

  return false;
}

static bool append_source_debug_symbol(SourceDebugSymbol symbol) {
  if (num_source_debug_symbols == source_debug_symbols_capacity) {
    size_t next_capacity =
        source_debug_symbols_capacity == 0 ? 8
                                           : source_debug_symbols_capacity * 2;
    SourceDebugSymbol *next =
        realloc(source_debug_symbols, next_capacity * sizeof(*next));
    if (next == NULL) {
      return false;
    }
    source_debug_symbols = next;
    source_debug_symbols_capacity = next_capacity;
  }

  source_debug_symbols[num_source_debug_symbols++] = symbol;
  return true;
}

static bool append_source_debug_call_jump(SourceDebugCallJump call_jump) {
  if (num_source_debug_call_jumps == source_debug_call_jumps_capacity) {
    size_t next_capacity =
        source_debug_call_jumps_capacity == 0
            ? 8
            : source_debug_call_jumps_capacity * 2;
    SourceDebugCallJump *next =
        realloc(source_debug_call_jumps, next_capacity * sizeof(*next));
    if (next == NULL) {
      return false;
    }
    source_debug_call_jumps = next;
    source_debug_call_jumps_capacity = next_capacity;
  }

  source_debug_call_jumps[num_source_debug_call_jumps++] = call_jump;
  return true;
}

static bool append_source_debug_return_address(uint32_t return_address) {
  if (num_source_debug_return_addresses ==
      source_debug_return_addresses_capacity) {
    size_t next_capacity = source_debug_return_addresses_capacity == 0
                               ? 8
                               : source_debug_return_addresses_capacity * 2;
    uint32_t *next =
        realloc(source_debug_return_addresses, next_capacity * sizeof(*next));
    if (next == NULL) {
      return false;
    }
    source_debug_return_addresses = next;
    source_debug_return_addresses_capacity = next_capacity;
  }

  source_debug_return_addresses[num_source_debug_return_addresses++] =
      return_address;
  return true;
}

static bool push_source_debug_stack_frame(const char *function_name,
                                          uint64_t baf) {
  if (source_debug_call_stack_size == source_debug_call_stack_capacity) {
    size_t next_capacity =
        source_debug_call_stack_capacity == 0
            ? 8
            : source_debug_call_stack_capacity * 2;
    SourceDebugStackFrame *next =
        realloc(source_debug_call_stack, next_capacity * sizeof(*next));
    if (next == NULL) {
      return false;
    }
    source_debug_call_stack = next;
    source_debug_call_stack_capacity = next_capacity;
  }

  source_debug_call_stack[source_debug_call_stack_size++] =
      (SourceDebugStackFrame){.function_name = function_name, .baf = baf};
  current_stackframe_function = function_name;
  return true;
}

static void pop_source_debug_stack_frame(void) {
  if (source_debug_call_stack_size > 0) {
    source_debug_call_stack_size--;
  }

  if (source_debug_call_stack_size > 0) {
    current_stackframe_function =
        source_debug_call_stack[source_debug_call_stack_size - 1].function_name;
  } else {
    current_stackframe_function = NULL;
  }
}

static SourceDebugStackFrame *current_source_debug_stack_frame(void) {
  if (source_debug_call_stack_size == 0) {
    return NULL;
  }
  return &source_debug_call_stack[source_debug_call_stack_size - 1];
}

static void load_source_debug_symbol_array(char *json, const char *array_key,
                                           bool is_argument) {
  char *array = find_json_array(json, array_key);
  for (char *cursor = array; cursor != NULL && *cursor != '\0'; cursor++) {
    cursor = skip_json_ws(cursor);
    if (*cursor == ']') {
      break;
    }
    if (*cursor != '{') {
      continue;
    }

    char *object_end = find_json_object_end(cursor);
    if (object_end == NULL) {
      break;
    }

    SourceDebugSymbol symbol = {
        .name = json_string_field(cursor, object_end, "name"),
        .scope = json_string_field(cursor, object_end, "scope"),
        .address = 0,
        .size = 1,
        .is_argument = is_argument,
    };
    bool has_address =
        json_u32_field(cursor, object_end, "address", &symbol.address);
    json_u32_field(cursor, object_end, "size", &symbol.size);

    if (symbol.name != NULL && symbol.scope != NULL && has_address &&
        symbol.size > 0) {
      if (!append_source_debug_symbol(symbol)) {
        free(symbol.name);
        free(symbol.scope);
        return;
      }
    } else {
      free(symbol.name);
      free(symbol.scope);
    }

    cursor = object_end;
  }
}

static void load_source_debug_symbols_from_json(char *json) {
  load_source_debug_symbol_array(json, "variables", false);
  load_source_debug_symbol_array(json, "arguments", true);

  char *call_jumps = find_json_array(json, "call_jumps");
  for (char *cursor = call_jumps; cursor != NULL && *cursor != '\0'; cursor++) {
    cursor = skip_json_ws(cursor);
    if (*cursor == ']') {
      break;
    }
    if (*cursor != '{') {
      continue;
    }

    char *object_end = find_json_object_end(cursor);
    if (object_end == NULL) {
      break;
    }

    SourceDebugCallJump call_jump = {
        .address = 0,
        .target_function =
            json_string_field(cursor, object_end, "target_function"),
    };
    bool has_address =
        json_u32_field(cursor, object_end, "address", &call_jump.address);

    if (call_jump.target_function != NULL && has_address) {
      if (!append_source_debug_call_jump(call_jump)) {
        free(call_jump.target_function);
        return;
      }
    } else {
      free(call_jump.target_function);
    }

    cursor = object_end;
  }

  char *return_addresses = find_json_array(json, "return_addresses");
  for (char *cursor = return_addresses;
       cursor != NULL && *cursor != '\0'; cursor++) {
    cursor = skip_json_ws(cursor);
    if (*cursor == ']') {
      break;
    }

    char *endptr = NULL;
    unsigned long parsed = strtoul(cursor, &endptr, 10);
    if (endptr == cursor || parsed > UINT32_MAX) {
      break;
    }
    if (!append_source_debug_return_address((uint32_t)parsed)) {
      break;
    }
    cursor = endptr;
  }
}

static void ensure_source_debug_symbols_loaded(void) {
  if (source_debug_symbols_load_attempted) {
    return;
  }
  source_debug_symbols_load_attempted = true;

  char *debuginfo_path = build_debuginfo_path();
  if (debuginfo_path == NULL) {
    return;
  }

  char *json = read_text_file_or_null(debuginfo_path);
  free(debuginfo_path);
  if (json == NULL) {
    return;
  }

  load_source_debug_symbols_from_json(json);
  free(json);
}

static bool is_global_scope(const char *scope) {
  return strcmp(scope, "global") == 0 || strcmp(scope, "<global>") == 0 ||
         strcmp(scope, "") == 0;
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

void source_debug_update_current_stackframe_function(void) {
  ensure_source_debug_symbols_loaded();
  if (regs == NULL) {
    return;
  }

  uint32_t pc = read_array(regs, PC, false);
  uint32_t cs = read_array(regs, CS, false);
  uint32_t relative_pc = pc >= cs ? pc - cs : pc;
  if (source_debug_last_event_valid && source_debug_last_event_pc == pc &&
      source_debug_last_event_cs == cs) {
    return;
  }

  for (size_t i = 0; i < num_source_debug_call_jumps; i++) {
    if (source_debug_call_jumps[i].address == relative_pc) {
      push_source_debug_stack_frame(source_debug_call_jumps[i].target_function,
                                    read_array(regs, BAF, false));
      source_debug_last_event_valid = true;
      source_debug_last_event_pc = pc;
      source_debug_last_event_cs = cs;
      return;
    }
  }

  for (size_t i = 0; i < num_source_debug_return_addresses; i++) {
    if (source_debug_return_addresses[i] == relative_pc) {
      pop_source_debug_stack_frame();
      source_debug_last_event_valid = true;
      source_debug_last_event_pc = pc;
      source_debug_last_event_cs = cs;
      return;
    }
  }
}

const char *source_debug_variable_label_for_sram_idx(uint64_t idx) {
  static char label[256];
  label[0] = '\0';

  ensure_source_debug_symbols_loaded();
  if (regs == NULL) {
    return NULL;
  }

  uint64_t ds = read_array(regs, DS, false);
  SourceDebugStackFrame *frame = current_source_debug_stack_frame();
  uint64_t baf = frame != NULL ? frame->baf : read_array(regs, BAF, false);
  bool wrote_any = false;

  if (frame != NULL) {
    if (((baf + 1) & 0x7FFFFFFF) == idx) {
      strcat(label, "[pr. stackfr. addr.");
      wrote_any = true;
    }
    if (((baf + 2) & 0x7FFFFFFF) == idx) {
      strcat(label, wrote_any ? ", return addr." : "[return addr.");
      wrote_any = true;
    }
  }

  for (size_t i = 0; i < num_source_debug_symbols; i++) {
    SourceDebugSymbol *symbol = &source_debug_symbols[i];
    bool is_global = is_global_scope(symbol->scope);

    if (!is_global &&
        (current_stackframe_function == NULL ||
         strcmp(symbol->scope, current_stackframe_function) != 0)) {
      continue;
    }

    for (uint32_t offset = 0; offset < symbol->size; offset++) {
      uint64_t symbol_addr;
      if (is_global) {
        symbol_addr = ds + symbol->address + offset;
      } else if (symbol->is_argument) {
        symbol_addr = baf + 3 + symbol->address + offset;
      } else {
        symbol_addr = baf - symbol->address - offset;
      }

      if ((symbol_addr & 0x7FFFFFFF) != idx) {
        continue;
      }

      char entry[96];
      const char *kind = symbol->is_argument ? "arg" : "var";
      if (is_global) {
        kind = "global";
      }
      if (symbol->size > 1) {
        snprintf(entry, sizeof(entry), "%s%s %s@%u+%u/%u",
                 wrote_any ? ", " : "[", kind, symbol->name, symbol->address,
                 offset, symbol->size);
      } else {
        snprintf(entry, sizeof(entry), "%s%s %s@%u", wrote_any ? ", " : "[",
                 kind, symbol->name, symbol->address);
      }

      size_t label_len = strlen(label);
      size_t entry_len = strlen(entry);
      if (label_len + entry_len + 2 >= sizeof(label)) {
        if (label_len + 4 < sizeof(label)) {
          strcat(label, "...");
        }
        return label;
      }

      strcat(label, entry);
      wrote_any = true;
    }
  }

  if (!wrote_any) {
    return NULL;
  }

  strcat(label, "]");
  return label;
}

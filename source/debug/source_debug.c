#include "../../include/source_debug.h"
#include "../../include/assemble.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include "../../include/utils.h"
#include "../../vendor/cJSON/cJSON.h"
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <unistd.h>

static pid_t source_debugger_pid = -1;

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

static bool write_source_debug_state_file(uint32_t pc, uint32_t cs) {
  char *state_path =
      build_reti_emulator_file_path(peripherals_dir, "source_debug_state.bin");
  char *temporary_state_path = build_reti_emulator_file_path(
      peripherals_dir, "source_debug_state.bin.tmp");
  int state_fd =
      open(temporary_state_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (state_fd < 0) {
    free(state_path);
    free(temporary_state_path);
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
  if (success && rename(temporary_state_path, state_path) != 0) {
    success = false;
  }
  if (!success) {
    unlink(temporary_state_path);
  }
  free(state_path);
  free(temporary_state_path);
  return success;
}

void write_source_debug_state(void) {
  if (regs == NULL || !ensure_reti_emulator_directory(peripherals_dir)) {
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
  if (strcmp(debuginfo_path, "") != 0) {
    return allocate_and_copy_string(debuginfo_path);
  }

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

static char *cjson_string_field(cJSON *object, const char *key) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!cJSON_IsString(item) || item->valuestring == NULL) {
    return NULL;
  }

  return allocate_and_copy_string(item->valuestring);
}

static bool cjson_u32_value(cJSON *item, uint32_t *value) {
  if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
      item->valuedouble > UINT32_MAX ||
      item->valuedouble != (uint32_t)item->valuedouble) {
    return false;
  }

  *value = (uint32_t)item->valuedouble;
  return true;
}

static bool cjson_u32_field(cJSON *object, const char *key, uint32_t *value) {
  return cjson_u32_value(cJSON_GetObjectItemCaseSensitive(object, key), value);
}

static void load_source_debug_symbol_array(cJSON *root, const char *array_key,
                                           bool is_argument) {
  cJSON *array = cJSON_GetObjectItemCaseSensitive(root, array_key);
  if (!cJSON_IsArray(array)) {
    return;
  }

  cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, array) {
    if (!cJSON_IsObject(entry)) {
      continue;
    }

    SourceDebugSymbol symbol = {
        .name = cjson_string_field(entry, "name"),
        .scope = cjson_string_field(entry, "scope"),
        .address = 0,
        .size = 1,
        .is_argument = is_argument,
    };
    bool has_address = cjson_u32_field(entry, "address", &symbol.address);
    cjson_u32_field(entry, "size", &symbol.size);

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
  }
}

static void load_source_debug_symbols_from_json(cJSON *root) {
  load_source_debug_symbol_array(root, "variables", false);
  load_source_debug_symbol_array(root, "arguments", true);

  cJSON *call_jumps = cJSON_GetObjectItemCaseSensitive(root, "call_jumps");
  if (!cJSON_IsArray(call_jumps)) {
    call_jumps = NULL;
  }
  cJSON *entry = NULL;
  cJSON_ArrayForEach(entry, call_jumps) {
    if (!cJSON_IsObject(entry)) {
      continue;
    }

    SourceDebugCallJump call_jump = {
        .address = 0,
        .target_function = cjson_string_field(entry, "target_function"),
    };
    bool has_address = cjson_u32_field(entry, "address", &call_jump.address);

    if (call_jump.target_function != NULL && has_address) {
      if (!append_source_debug_call_jump(call_jump)) {
        free(call_jump.target_function);
        return;
      }
    } else {
      free(call_jump.target_function);
    }
  }

  cJSON *return_addresses =
      cJSON_GetObjectItemCaseSensitive(root, "return_addresses");
  if (!cJSON_IsArray(return_addresses)) {
    return_addresses = NULL;
  }
  cJSON_ArrayForEach(entry, return_addresses) {
    uint32_t return_address;
    if (!cjson_u32_value(entry, &return_address)) {
      continue;
    }
    if (!append_source_debug_return_address(return_address)) {
      break;
    }
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

  cJSON *root = cJSON_Parse(json);
  if (root != NULL) {
    load_source_debug_symbols_from_json(root);
    cJSON_Delete(root);
  }
  free(json);
}

static bool is_global_scope(const char *scope) {
  return strcmp(scope, "global") == 0 || strcmp(scope, "<global>") == 0 ||
         strcmp(scope, "") == 0;
}

bool start_source_debugger(void) {
  reap_source_debugger_if_exited();
  if (source_debugger_pid > 0) {
    return true;
  }
  if (!ensure_reti_emulator_directory(peripherals_dir)) {
    return false;
  }

  activate_source_debug();
  write_source_debug_state();

  char *helper_path = build_debug_helper_path("source_debug");
  char *script_path = build_debug_script_path("source_debug.py");
  char *debuginfo_path = build_debuginfo_path();
  char *state_path =
      build_reti_emulator_file_path(peripherals_dir, "source_debug_state.bin");
  if (script_path == NULL || debuginfo_path == NULL || state_path == NULL) {
    free(helper_path);
    free(script_path);
    free(debuginfo_path);
    free(state_path);
    return false;
  }

  pid_t child_pid = fork();
  if (child_pid < 0) {
    free(helper_path);
    free(script_path);
    free(debuginfo_path);
    free(state_path);
    return false;
  }

  if (child_pid == 0) {
#ifdef __linux__
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0) {
      _exit(EXIT_FAILURE);
    }
#endif
    if (helper_path != NULL && access(helper_path, X_OK) == 0) {
      execl(helper_path, helper_path, debuginfo_path, state_path, NULL);
    }
    execlp("python3", "python3", script_path, debuginfo_path, state_path,
           NULL);
    fprintf(stderr,
            "Source debugger unavailable: install Python 3 with tkinter\n");
    _exit(EXIT_FAILURE);
  }

  free(helper_path);
  free(script_path);
  free(debuginfo_path);
  free(state_path);
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

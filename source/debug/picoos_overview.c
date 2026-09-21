#include "../../include/picoos_overview.h"
#include "../../include/assemble.h"
#include "../../include/core_debug.h"
#include "../../include/exception.h"
#include "../../include/interrupt_controller.h"
#include "../../include/parse/parse_args.h"
#include "../../include/reti.h"
#include "../../include/statemachine.h"
#include "../../include/utils.h"
#include "../../vendor/cJSON/cJSON.h"
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef __linux__
#include <sys/prctl.h>
#endif
#include <unistd.h>

#define OVERVIEW_EVENT_CAPACITY 128
#define OVERVIEW_EVENT_TEXT_CAPACITY 512
#define OVERVIEW_ARGUMENT_WORDS 8
#define OVERVIEW_SOURCE_CAPACITY 32

typedef struct {
  uint64_t sequence;
  uint32_t repeat_count;
  char kind[24];
  char source[OVERVIEW_SOURCE_CAPACITY];
  char text[OVERVIEW_EVENT_TEXT_CAPACITY];
  uint32_t pc;
  uint32_t isr;
  uint32_t number;
  uint32_t argument;
  uint32_t detail;
  uint32_t argument_words[OVERVIEW_ARGUMENT_WORDS];
  size_t argument_word_count;
} PicoOSOverviewEvent;

typedef struct {
  char source[OVERVIEW_SOURCE_CAPACITY];
  uint32_t isr;
  uint32_t syscall_number;
  uint32_t argument;
} PicoOSOverviewHandler;

static pid_t overview_pid = -1;
static char *overview_navigation_path = NULL;
static Program_Sections overview_sections;
static bool overview_sections_set = false;
static uint64_t overview_generation = 0;
static uint64_t overview_event_sequence = 0;
static PicoOSOverviewEvent overview_events[OVERVIEW_EVENT_CAPACITY];
static size_t overview_event_start = 0;
static size_t overview_event_count = 0;
static PicoOSOverviewHandler overview_handlers[MAX_STACK_SIZE];
static size_t overview_handler_count = 0;
static char pending_interrupt_sources[NUM_ISR_SLOTS][OVERVIEW_SOURCE_CAPACITY];

static bool file_is_readable(const char *path) {
  return path != NULL && access(path, R_OK) == 0;
}

static char *read_text_file(const char *path) {
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

static char *replace_path_extension(const char *path, const char *extension) {
  if (path == NULL || path[0] == '\0') {
    return NULL;
  }

  const char *last_slash = strrchr(path, '/');
  const char *filename = last_slash == NULL ? path : last_slash + 1;
  const char *last_dot = strrchr(filename, '.');
  size_t basename_len =
      last_dot == NULL ? strlen(path) : (size_t)(last_dot - path);
  char *result = malloc(basename_len + strlen(extension) + 1);
  if (result == NULL) {
    return NULL;
  }

  memcpy(result, path, basename_len);
  strcpy(result + basename_len, extension);
  return result;
}

static char *overview_debuginfo_path(void) {
  if (strcmp(debuginfo_path, "") != 0) {
    return allocate_and_copy_string(debuginfo_path);
  }
  return replace_path_extension(sram_prgrm_path, ".debuginfo");
}

static char *overview_layout_path(void) {
  char *debug_path = overview_debuginfo_path();
  char *result = replace_path_extension(debug_path, ".overview");
  free(debug_path);
  return result;
}

static char *overview_override_path(void) {
  char *debug_path = overview_debuginfo_path();
  char *result = replace_path_extension(debug_path, ".overview.override.json");
  free(debug_path);
  return result;
}

static void reap_overview_if_exited(void) {
  if (overview_pid <= 0) {
    return;
  }
  if (waitpid(overview_pid, NULL, WNOHANG) == overview_pid) {
    overview_pid = -1;
  }
}

static void remove_navigation_files(void) {
  if (overview_navigation_path == NULL) {
    return;
  }

  size_t path_len = strlen(overview_navigation_path);
  char *temporary_path = malloc(path_len + strlen(".tmp") + 1);
  char *processing_path = malloc(path_len + strlen(".processing") + 1);
  if (temporary_path != NULL) {
    sprintf(temporary_path, "%s.tmp", overview_navigation_path);
    unlink(temporary_path);
  }
  if (processing_path != NULL) {
    sprintf(processing_path, "%s.processing", overview_navigation_path);
    unlink(processing_path);
  }
  unlink(overview_navigation_path);
  free(temporary_path);
  free(processing_path);
}

static PicoOSOverviewEvent *append_event(const char *kind, const char *source) {
  size_t index;
  if (overview_event_count < OVERVIEW_EVENT_CAPACITY) {
    index =
        (overview_event_start + overview_event_count) % OVERVIEW_EVENT_CAPACITY;
    overview_event_count++;
  } else {
    index = overview_event_start;
    overview_event_start = (overview_event_start + 1) % OVERVIEW_EVENT_CAPACITY;
  }

  PicoOSOverviewEvent *event = &overview_events[index];
  memset(event, 0, sizeof(*event));
  event->sequence = ++overview_event_sequence;
  event->repeat_count = 1;
  snprintf(event->kind, sizeof(event->kind), "%s", kind);
  snprintf(event->source, sizeof(event->source), "%s", source);
  event->pc = regs == NULL ? 0 : read_array(regs, PC, false);
  event->isr = UINT32_MAX;
  return event;
}

static void push_handler(const char *source, uint32_t isr,
                         uint32_t syscall_number, uint32_t argument) {
  if (overview_handler_count >= MAX_STACK_SIZE) {
    return;
  }
  PicoOSOverviewHandler *handler = &overview_handlers[overview_handler_count++];
  snprintf(handler->source, sizeof(handler->source), "%s", source);
  handler->isr = isr;
  handler->syscall_number = syscall_number;
  handler->argument = argument;
}

static bool sram_address_is_valid(uint32_t address) {
  return address >> 30 >= SRAM_CONST && (address & 0x7fffffff) < sram_size;
}

static void capture_argument_words(PicoOSOverviewEvent *event) {
  if (!sram_address_is_valid(event->argument)) {
    return;
  }
  uint32_t index = event->argument & 0x7fffffff;
  size_t count = OVERVIEW_ARGUMENT_WORDS;
  if (count > sram_size - index) {
    count = sram_size - index;
  }
  for (size_t i = 0; i < count; i++) {
    event->argument_words[i] = read_storage(event->argument + (uint32_t)i);
  }
  event->argument_word_count = count;
}

void set_picoos_overview_sections(Program_Sections sections) {
  overview_sections = sections;
  overview_sections_set = true;
}

void reset_picoos_overview(bool synthetic_startup) {
  overview_generation = 0;
  overview_event_sequence = 0;
  overview_event_start = 0;
  overview_event_count = 0;
  overview_handler_count = 0;
  memset(pending_interrupt_sources, 0, sizeof(pending_interrupt_sources));
  if (synthetic_startup) {
    PicoOSOverviewEvent *event = append_event("interrupt", "INIT");
    snprintf(event->text, sizeof(event->text), "Synthetic OS startup");
    push_handler("INIT", UINT32_MAX, UINT32_MAX, 0);
  }
}

void picoos_overview_log_software_interrupt(uint8_t isr,
                                            uint32_t syscall_number,
                                            uint32_t argument) {
  const char *kind = isr == 0 ? "syscall" : "software_interrupt";
  const char *source = isr == 0 ? "syscall" : "software";
  PicoOSOverviewEvent *event = append_event(kind, source);
  event->isr = isr;
  event->number = syscall_number;
  event->argument = argument;
  capture_argument_words(event);
  push_handler(source, isr, syscall_number, argument);
}

void picoos_overview_log_hardware_interrupt(const char *source, uint8_t isr,
                                            uint32_t detail) {
  if (strcmp(source, "Timer") == 0 && overview_event_count > 0) {
    size_t latest_index = (overview_event_start + overview_event_count - 1) %
                          OVERVIEW_EVENT_CAPACITY;
    PicoOSOverviewEvent *latest = &overview_events[latest_index];
    if (strcmp(latest->kind, "hardware_interrupt") == 0 &&
        strcmp(latest->source, source) == 0 && latest->isr == isr) {
      latest->repeat_count++;
      latest->pc = regs == NULL ? 0 : read_array(regs, PC, false);
      latest->detail = detail;
      snprintf(pending_interrupt_sources[isr], OVERVIEW_SOURCE_CAPACITY, "%s",
               source);
      return;
    }
  }

  PicoOSOverviewEvent *event = append_event("hardware_interrupt", source);
  event->isr = isr;
  event->detail = detail;
  snprintf(pending_interrupt_sources[isr], OVERVIEW_SOURCE_CAPACITY, "%s",
           source);
}

void picoos_overview_enter_hardware_interrupt(uint8_t isr) {
  const char *source = pending_interrupt_sources[isr][0] == '\0'
                           ? "hardware"
                           : pending_interrupt_sources[isr];
  push_handler(source, isr, UINT32_MAX, 0);
  pending_interrupt_sources[isr][0] = '\0';
}

void picoos_overview_log_cpu_exception(uint8_t isr, uint32_t cause) {
  PicoOSOverviewEvent *event = append_event("cpu_exception", "CPU exception");
  event->isr = isr;
  event->detail = cause;
  push_handler("CPU exception", isr, UINT32_MAX, 0);
}

void picoos_overview_interrupt_returned(void) {
  if (overview_handler_count > 0) {
    overview_handler_count--;
  }
}

void picoos_overview_log_host_request(const char *request) {
  PicoOSOverviewEvent *event = append_event("host_request", "UART escape");
  snprintf(event->text, sizeof(event->text), "%s", request);
}

static bool layout_identifies_loaded_picoos(const char *layout_path) {
  if (!overview_sections_set || !overview_sections.exists || regs == NULL ||
      sram == NULL || !file_is_readable(layout_path)) {
    return false;
  }

  char *json = read_text_file(layout_path);
  if (json == NULL) {
    return false;
  }
  cJSON *root = cJSON_Parse(json);
  free(json);
  if (root == NULL) {
    return false;
  }

  cJSON *system = cJSON_GetObjectItemCaseSensitive(root, "system");
  cJSON *globals = cJSON_GetObjectItemCaseSensitive(root, "globals");
  cJSON *vector = cJSON_GetObjectItemCaseSensitive(root, "interrupt_vector");
  cJSON *labels = cJSON_GetObjectItemCaseSensitive(root, "labels");
  const char *required_globals[] = {
      "interrupt_vector_table",  "kernel_heap",       "process_memory_heap",
      "shared_memory_list_head", "process_list_head", "active_process"};
  bool valid = cJSON_IsString(system) &&
               strcmp(system->valuestring, "PicoOS") == 0 &&
               cJSON_IsObject(globals) && cJSON_IsArray(vector) &&
               cJSON_IsObject(labels);

  for (size_t i = 0;
       valid && i < sizeof(required_globals) / sizeof(required_globals[0]);
       i++) {
    valid = cJSON_IsObject(
        cJSON_GetObjectItemCaseSensitive(globals, required_globals[i]));
  }

  int vector_count = cJSON_GetArraySize(vector);
  valid = valid && vector_count > 0 && vector_count <= isr_num &&
          (uint32_t)vector_count <= overview_sections.codesegment_start;
  for (int i = 0; valid && i < vector_count; i++) {
    cJSON *handler_name = cJSON_GetArrayItem(vector, i);
    cJSON *handler_address = cJSON_IsString(handler_name)
                                 ? cJSON_GetObjectItemCaseSensitive(
                                       labels, handler_name->valuestring)
                                 : NULL;
    uint32_t vector_value = read_file(sram, (uint64_t)i);
    uint32_t vector_address = vector_value & 0x7fffffff;
    valid = cJSON_IsNumber(handler_address) &&
            vector_value >> 30 >= SRAM_CONST &&
            vector_address == (uint32_t)handler_address->valuedouble &&
            vector_address >= overview_sections.codesegment_start &&
            vector_address < overview_sections.datasegment_start;
  }

  cJSON *process_heap =
      cJSON_GetObjectItemCaseSensitive(globals, "process_memory_heap");
  cJSON *process_heap_offset =
      cJSON_IsObject(process_heap)
          ? cJSON_GetObjectItemCaseSensitive(process_heap, "address")
          : NULL;
  uint32_t pc = read_array(regs, PC, false);
  uint32_t pc_index = pc & 0x7fffffff;
  bool executing_kernel = pc >> 30 >= SRAM_CONST &&
                          pc_index >= overview_sections.codesegment_start &&
                          pc_index < overview_sections.datasegment_start;
  bool process_heap_initialized = false;
  if (cJSON_IsNumber(process_heap_offset)) {
    uint32_t heap_global_address = overview_sections.datasegment_start +
                                   (uint32_t)process_heap_offset->valuedouble;
    uint32_t first_process_block = read_file(sram, heap_global_address);
    uint32_t first_process_block_index = first_process_block & 0x7fffffff;
    process_heap_initialized =
        first_process_block >> 30 >= SRAM_CONST &&
        first_process_block_index > overview_sections.stack_start &&
        first_process_block_index < sram_size;
  }
  valid = valid && (executing_kernel || process_heap_initialized);

  cJSON_Delete(root);
  return valid;
}

bool picoos_overview_is_available(void) {
  char *layout_path = overview_layout_path();
  char *debug_path = overview_debuginfo_path();
  bool available = file_is_readable(debug_path) &&
                   layout_identifies_loaded_picoos(layout_path);
  free(layout_path);
  free(debug_path);
  return available;
}

static cJSON *sections_json(void) {
  cJSON *sections = cJSON_CreateObject();
  cJSON_AddNumberToObject(sections, "interrupt_service_routines_start",
                          overview_sections.interrupt_service_routines_start);
  cJSON_AddNumberToObject(sections, "codesegment_start",
                          overview_sections.codesegment_start);
  cJSON_AddNumberToObject(sections, "datasegment_start",
                          overview_sections.datasegment_start);
  cJSON_AddNumberToObject(sections, "heap_start", overview_sections.heap_start);
  cJSON_AddNumberToObject(sections, "heap_size", overview_sections.heap_size);
  cJSON_AddNumberToObject(sections, "stack_start",
                          overview_sections.stack_start);
  return sections;
}

static cJSON *registers_json(void) {
  cJSON *values = cJSON_CreateObject();
  const char *names[] = {"PC", "IN1", "IN2", "ACC", "SP", "BAF", "CS", "DS"};
  for (uint8_t i = 0; i < NUM_REGISTERS; i++) {
    cJSON_AddNumberToObject(values, names[i], read_array(regs, i, false));
  }
  return values;
}

static cJSON *interrupts_json(void) {
  cJSON *interrupts = cJSON_CreateObject();
  cJSON_AddNumberToObject(interrupts, "vector_count", isr_num);
  cJSON_AddNumberToObject(interrupts, "stacked_count", stacked_isrs_cnt);
  cJSON_AddNumberToObject(interrupts, "cpu_exception_cause",
                          cpu_exception_cause);

  cJSON *devices = cJSON_AddArrayToObject(interrupts, "devices");
  const char *device_names[] = {"timer", "custom/DMA", "UART"};
  sync_interrupt_controller_from_memory();
  for (uint8_t i = 0; i < NUM_HARDWARE_INTERRUPT_SIGNAL_LINES; i++) {
    cJSON *device = cJSON_CreateObject();
    cJSON_AddStringToObject(device, "name", device_names[i]);
    cJSON_AddNumberToObject(device, "isr", device_to_isr[i]);
    cJSON_AddNumberToObject(device, "priority", device_to_prio[i]);
    cJSON_AddItemToArray(devices, device);
  }

  cJSON *active = cJSON_AddArrayToObject(interrupts, "active");
  for (size_t i = 0; i < overview_handler_count; i++) {
    PicoOSOverviewHandler *handler = &overview_handlers[i];
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddStringToObject(entry, "source", handler->source);
    cJSON_AddNumberToObject(entry, "isr", handler->isr);
    cJSON_AddNumberToObject(entry, "syscall_number", handler->syscall_number);
    cJSON_AddNumberToObject(entry, "argument", handler->argument);
    cJSON_AddItemToArray(active, entry);
  }
  return interrupts;
}

static cJSON *events_json(void) {
  cJSON *events = cJSON_CreateArray();
  for (size_t i = 0; i < overview_event_count; i++) {
    size_t index = (overview_event_start + i) % OVERVIEW_EVENT_CAPACITY;
    PicoOSOverviewEvent *event = &overview_events[index];
    cJSON *entry = cJSON_CreateObject();
    cJSON_AddNumberToObject(entry, "sequence", (double)event->sequence);
    cJSON_AddNumberToObject(entry, "repeat_count", event->repeat_count);
    cJSON_AddStringToObject(entry, "kind", event->kind);
    cJSON_AddStringToObject(entry, "source", event->source);
    cJSON_AddStringToObject(entry, "text", event->text);
    cJSON_AddNumberToObject(entry, "pc", event->pc);
    cJSON_AddNumberToObject(entry, "isr", event->isr);
    cJSON_AddNumberToObject(entry, "number", event->number);
    cJSON_AddNumberToObject(entry, "argument", event->argument);
    cJSON_AddNumberToObject(entry, "detail", event->detail);
    cJSON *words = cJSON_AddArrayToObject(entry, "argument_words");
    for (size_t word = 0; word < event->argument_word_count; word++) {
      cJSON_AddItemToArray(words,
                           cJSON_CreateNumber(event->argument_words[word]));
    }
    cJSON_AddItemToArray(events, entry);
  }
  return events;
}

static bool write_all(int fd, const char *content, size_t size) {
  size_t offset = 0;
  while (offset < size) {
    ssize_t written = write(fd, content + offset, size - offset);
    if (written <= 0) {
      return false;
    }
    offset += (size_t)written;
  }
  return true;
}

static bool write_overview_state(void) {
  if (regs == NULL || !ensure_reti_emulator_directory(peripherals_dir)) {
    return false;
  }

  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "format_version", 1);
  cJSON_AddNumberToObject(root, "generation", (double)++overview_generation);
  cJSON_AddNumberToObject(root, "sram_size", sram_size);
  cJSON_AddBoolToObject(root, "paused",
                       breakpoint_encountered && isr_finished && isr_step_into);
  cJSON_AddBoolToObject(root, "active_sram_box",
                       active_debug_box_is_sram());
  cJSON_AddItemToObject(root, "registers", registers_json());
  cJSON_AddItemToObject(root, "sections", sections_json());
  cJSON_AddItemToObject(root, "interrupts", interrupts_json());
  cJSON_AddItemToObject(root, "events", events_json());
  char *json = cJSON_PrintUnformatted(root);
  cJSON_Delete(root);
  if (json == NULL) {
    return false;
  }

  char *state_path = build_reti_emulator_file_path(
      peripherals_dir, "picoos_overview_state.json");
  char *temporary_path = build_reti_emulator_file_path(
      peripherals_dir, "picoos_overview_state.json.tmp");
  int fd = open(temporary_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
  bool success = fd >= 0 && write_all(fd, json, strlen(json)) && fsync(fd) == 0;
  if (fd >= 0 && close(fd) != 0) {
    success = false;
  }
  if (success && rename(temporary_path, state_path) != 0) {
    success = false;
  }
  if (!success) {
    unlink(temporary_path);
  }

  free(json);
  free(state_path);
  free(temporary_path);
  return success;
}

bool start_picoos_overview(void) {
  reap_overview_if_exited();
  if (overview_pid > 0) {
    return true;
  }
  if (!picoos_overview_is_available() || !write_overview_state()) {
    return false;
  }

  char *helper_path = build_debug_helper_path("picoos_overview");
  char *script_path = build_debug_script_path("picoos_overview.py");
  char *debug_path = overview_debuginfo_path();
  char *layout_path = overview_layout_path();
  char *override_path = overview_override_path();
  char *state_path = build_reti_emulator_file_path(
      peripherals_dir, "picoos_overview_state.json");
  char *sram_path = build_reti_emulator_file_path(peripherals_dir, "sram.bin");
  char *navigation_path = build_reti_emulator_file_path(
      peripherals_dir, "picoos_overview_navigation.txt");
  if (script_path == NULL || debug_path == NULL || layout_path == NULL ||
      override_path == NULL || state_path == NULL || sram_path == NULL ||
      navigation_path == NULL) {
    free(helper_path);
    free(script_path);
    free(debug_path);
    free(layout_path);
    free(override_path);
    free(state_path);
    free(sram_path);
    free(navigation_path);
    return false;
  }

  remove_navigation_files();
  free(overview_navigation_path);
  overview_navigation_path = navigation_path;
  remove_navigation_files();

  pid_t child_pid = fork();
  if (child_pid == 0) {
#ifdef __linux__
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0) {
      _exit(EXIT_FAILURE);
    }
#endif
    if (helper_path != NULL && access(helper_path, X_OK) == 0) {
      execl(helper_path, helper_path, debug_path, layout_path, override_path,
            state_path, sram_path, overview_navigation_path, NULL);
    }
    execlp("python3", "python3", script_path, debug_path, layout_path,
           override_path, state_path, sram_path, overview_navigation_path,
           NULL);
    fprintf(stderr, "PicoOS Overview unavailable: install Python 3 with "
                    "tkinter\n");
    _exit(EXIT_FAILURE);
  }

  free(helper_path);
  free(script_path);
  free(debug_path);
  free(layout_path);
  free(override_path);
  free(state_path);
  free(sram_path);
  if (child_pid < 0) {
    remove_navigation_files();
    free(overview_navigation_path);
    overview_navigation_path = NULL;
    return false;
  }
  overview_pid = child_pid;
  return true;
}

bool picoos_overview_is_open(void) {
  reap_overview_if_exited();
  return overview_pid > 0;
}

bool picoos_overview_take_navigation_request(uint32_t *start, uint32_t *end) {
  reap_overview_if_exited();
  if (overview_pid <= 0 || overview_navigation_path == NULL) {
    return false;
  }

  size_t processing_len =
      strlen(overview_navigation_path) + strlen(".processing") + 1;
  char *processing_path = malloc(processing_len);
  if (processing_path == NULL) {
    return false;
  }
  snprintf(processing_path, processing_len, "%s.processing",
           overview_navigation_path);
  unlink(processing_path);
  if (rename(overview_navigation_path, processing_path) != 0) {
    free(processing_path);
    return false;
  }

  char *request = read_text_file(processing_path);
  unlink(processing_path);
  free(processing_path);
  if (request == NULL) {
    return false;
  }

  unsigned long long parsed_start;
  unsigned long long parsed_end;
  bool valid = sscanf(request, "%llu %llu", &parsed_start, &parsed_end) == 2 &&
               parsed_start <= parsed_end && parsed_end < sram_size;
  free(request);
  if (!valid) {
    return false;
  }

  *start = (uint32_t)parsed_start;
  *end = (uint32_t)parsed_end;
  return true;
}

void refresh_picoos_overview(void) {
  reap_overview_if_exited();
  if (overview_pid > 0) {
    write_overview_state();
  }
}

void stop_picoos_overview(void) {
  reap_overview_if_exited();
  if (overview_pid > 0) {
    kill(overview_pid, SIGTERM);
    waitpid(overview_pid, NULL, 0);
    overview_pid = -1;
  }
  remove_navigation_files();
  free(overview_navigation_path);
  overview_navigation_path = NULL;
}

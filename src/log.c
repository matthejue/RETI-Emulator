#include "../include/assemble.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/statemachine.h"
#include <errno.h>
#include <fcntl.h> // open()
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // mkdir, stat
#include <sys/types.h> // mkdir, stat
#include <time.h>
#include <unistd.h> // close()

#define MAX_HEAP_INDEX 5
#define MAX_STACK_INDEX 5

bool debug_activated = false;

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

static char *u8_array_to_space_separated(const uint8_t *arr, size_t n) {
  size_t cap = (n ? n * 4 : 1) + 1;
  char *buf = (char *)malloc(cap);

  char *p = buf;
  size_t remaining = cap;
  for (size_t i = 0; i < n; ++i) {
    int written =
        snprintf(p, remaining, (i + 1 < n) ? "%u " : "%u", (unsigned)arr[i]);
    if (written < 0 ||
        (size_t)written >= remaining) { // shouldn't happen given cap
      free(buf);
      return NULL;
    }
    p += written;
    remaining -= (size_t)written;
  }
  return buf; // caller frees
}

void log_variable(const char *file_name, const char *var_name,
                  const char *value_str) {
  FILE *file = fopen(file_name, "a");
  if (!file) {
    fprintf(stderr, "Error opening log file '%s': %s\n",
            file_name ? file_name : "(null)", strerror(errno));
    return;
  }
  fprintf(file, "%s: %s\n", var_name ? var_name : "(null)",
          value_str ? value_str : "(null)");
  if (fclose(file) == EOF) {
    fprintf(stderr, "Error closing log file '%s': %s\n", file_name,
            strerror(errno));
  }
}

#define LOG_U8(FILEPATH, KEY, VAL)                                             \
  do {                                                                         \
    char _buf[4]; /* "255" + NUL */                                            \
    snprintf(_buf, sizeof(_buf), "%hhu", (unsigned char)(VAL));                \
    log_variable((FILEPATH), (KEY), _buf);                                     \
  } while (0)

#define LOG_I8(FILEPATH, KEY, VAL)                                             \
  do {                                                                         \
    char _buf[5]; /* "-128" + NUL */                                           \
    snprintf(_buf, sizeof(_buf), "%hhd", (signed char)(VAL));                  \
    log_variable((FILEPATH), (KEY), _buf);                                     \
  } while (0)

#define LOG_BOOL(FILEPATH, KEY, VAL)                                           \
  log_variable((FILEPATH), (KEY), (VAL) ? "true" : "false")

#define LOG_HEX32(FILEPATH, KEY, VAL)                                          \
  do {                                                                         \
    char _buf[32];                                                             \
    snprintf(_buf, sizeof(_buf), "0x%08" PRIx32, (uint32_t)(VAL));             \
    log_variable((FILEPATH), (KEY), _buf);                                     \
  } while (0)

/* --- Event → string mapper so we can print the enum identifier --- */
static const char *event_to_string(Event e) {
  switch (e) {
  case CONTINUE:
    return "CONTINUE";
  case FINALIZE:
    return "FINALIZE";
  case BREAKPOINT_ENCOUNTERED:
    return "BREAKPOINT_ENCOUNTERED";
  case HARDWARE_INTERRUPT:
    return "HARDWARE_INTERRUPT";
  case RETURN_FROM_INTERRUPT:
    return "RETURN_FROM_INTERRUPT";
  case SOFTWARE_INTERRUPT:
    return "SOFTWARE_INTERRUPT";
  case STEP_INTO_ACTION:
    return "STEP_INTO_ACTION";
  default:
    return "UNKNOWN_EVENT";
  }
}

/* Session-stable logfile path */
static char s_logfile[512] = {0};

static int ensure_dir_exists(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0) {
    if (S_ISDIR(st.st_mode))
      return 0;
    fprintf(stderr, "Path exists but is not a directory: %s\n", path);
    errno = ENOTDIR;
    return -1;
  }
  if (errno != ENOENT) {
    fprintf(stderr, "stat('%s') failed: %s\n", path, strerror(errno));
    return -1;
  }
  if (mkdir(path, 0777) == -1 && errno != EEXIST) {
    fprintf(stderr, "mkdir('%s') failed: %s\n", path, strerror(errno));
    return -1;
  }
  return 0;
}

static int touch_file(const char *path) {
  int fd = open(path, O_CREAT | O_APPEND | O_WRONLY, 0666);
  if (fd == -1) {
    fprintf(stderr, "Error creating/opening log file '%s': %s\n", path,
            strerror(errno));
    return -1;
  }
  close(fd);
  return 0;
}

void log_statemachine(Event event) {
  const char *log_dir = "/tmp/reti_emulator";

  // Initialize the session logfile path once
  if (s_logfile[0] == '\0') {
    if (ensure_dir_exists(log_dir) != 0) {
      fprintf(stderr, "Cannot use log directory '%s'. Logging disabled.\n",
              log_dir);
      return;
    }

    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);

    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d_%H-%M-%S", &tm_now);

    snprintf(s_logfile, sizeof(s_logfile), "%s/statemachine_%s.log", log_dir,
             ts);

    if (touch_file(s_logfile) != 0) {
      s_logfile[0] = '\0';
      return;
    }
  }

  /* --- Use the single string logger with conversions at call-site --- */
  log_variable(s_logfile, "event",
               event_to_string(event)); // enum as identifier string

  // Adjust these based on your real types; here are typical choices:
  LOG_U8(s_logfile, "stacked_isrs_count", stacked_isrs_cnt);

  LOG_BOOL(s_logfile, "breakpoint_encountered", breakpoint_encountered);
  LOG_BOOL(s_logfile, "isr_finished", isr_finished);
  LOG_BOOL(s_logfile, "isr_step_into", isr_step_into);

  LOG_U8(s_logfile, "finished_isr_here", finished_isr_here);
  LOG_BOOL(s_logfile, "not_stepped_into_isr_here", not_stepped_into_isr_here);

  LOG_U8(s_logfile, "deactivated_keypress_interrupt_here",
         deactivated_keypress_interrupt_here);
  LOG_U8(s_logfile, "deactivated_timer_interrupt_here",
         deactivated_timer_interrupt_here);

  LOG_I8(s_logfile, "hardware_isr_stack_top", hardware_isr_stack_top);
  LOG_I8(s_logfile, "is_hardware_int_stack_top", is_hardware_int_stack_top);
  LOG_U8(s_logfile, "heap_size", heap_size);

  LOG_U8(s_logfile, "latest_isr", latest_isr);
  LOG_BOOL(s_logfile, "step_into_activated", step_into_activated);

  LOG_U8(s_logfile, "timer_cnt", timer_cnt);
  LOG_BOOL(s_logfile, "interrupt_timer_active", interrupt_timer_active);
  LOG_BOOL(s_logfile, "keypress_interrupt_active", keypress_interrupt_active);

  size_t stack_len = MIN((size_t)MAX_STACK_SIZE, (size_t)MAX_STACK_INDEX + 1);
  char *stack_str = u8_array_to_space_separated(hardware_isr_stack, stack_len);
  log_variable(s_logfile, "isr_stack", stack_str);
  free(stack_str);

  size_t heap_len = MIN((size_t)HEAP_SIZE, (size_t)MAX_HEAP_INDEX + 1);
  char *heap_str = u8_array_to_space_separated(isr_heap, heap_len);
  log_variable(s_logfile, "isr_heap", heap_str);
  free(heap_str);

  size_t prio_len = (size_t)isr_num; // number of entries
  if (isr_to_prio && prio_len > 0) {
    char *prio_str = u8_array_to_space_separated(isr_to_prio, prio_len);
    log_variable(s_logfile, "isr_to_prio", prio_str);
    free(prio_str);
  } else {
    log_variable(s_logfile, "isr_to_prio", "(null or empty)");
  }

  FILE *file = fopen(s_logfile, "a");
  if (!file) {
    fprintf(stderr, "Failed to open log file '%s': %s\n", s_logfile,
            strerror(errno));
    return;
  }
  fputc('\n', file);
  fclose(file);
}

void debug() {
  if (debug_activated) {
#ifdef __linux__
    __asm__("int3"); // ../.gdbinit
#endif
  }
}

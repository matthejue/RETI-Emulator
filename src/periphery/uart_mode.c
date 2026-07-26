#include "../../include/uart_mode.h"
#include "../../include/interrupt.h"
#include "../../include/parse/parse_args.h"
#include "../../include/terminal_view.h"
#include "../../include/tui.h"
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define UART_MODE_ESCAPE 27

static struct termios saved_stdin_termios;
static int saved_stdin_flags = 0;
static bool stdin_termios_saved = false;
static bool stdin_flags_saved = false;
static bool restore_at_exit_registered = false;
static bool termination_handlers_saved = false;
static void (*saved_sigint_handler)(int) = SIG_DFL;
static void (*saved_sigterm_handler)(int) = SIG_DFL;
static volatile sig_atomic_t termination_signal = 0;
static uint8_t *queued_input = NULL;
static size_t queued_input_len = 0;
static size_t queued_input_idx = 0;

static void clear_queued_input(void) {
  free(queued_input);
  queued_input = NULL;
  queued_input_len = 0;
  queued_input_idx = 0;
}

static void append_queued_input(uint8_t byte) {
  uint8_t *new_input = realloc(queued_input, queued_input_len + 1);
  if (new_input == NULL) {
    fprintf(stderr, "Error: Couldn't allocate (U)ART mode input buffer\n");
    exit(EXIT_FAILURE);
  }
  queued_input = new_input;
  queued_input[queued_input_len++] = byte;
}

static void restore_stdin(void) {
  if (stdin_termios_saved) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_stdin_termios);
    stdin_termios_saved = false;
  }
  if (stdin_flags_saved) {
    fcntl(STDIN_FILENO, F_SETFL, saved_stdin_flags);
    stdin_flags_saved = false;
  }
  if (termination_handlers_saved) {
    signal(SIGINT, saved_sigint_handler);
    signal(SIGTERM, saved_sigterm_handler);
    termination_handlers_saved = false;
  }
}

static void request_termination(int signal_number) {
  termination_signal = signal_number;
}

static bool prepare_stdin(void) {
  saved_stdin_flags = fcntl(STDIN_FILENO, F_GETFL);
  if (saved_stdin_flags < 0 ||
      fcntl(STDIN_FILENO, F_SETFL, saved_stdin_flags | O_NONBLOCK) < 0) {
    return false;
  }
  stdin_flags_saved = true;

  if (tcgetattr(STDIN_FILENO, &saved_stdin_termios) == 0) {
    struct termios uart_termios = saved_stdin_termios;
    uart_termios.c_iflag &= ~(ICRNL | IXON);
    uart_termios.c_lflag &= ~(ICANON | ECHO);
    uart_termios.c_cc[VMIN] = 0;
    uart_termios.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &uart_termios) < 0) {
      restore_stdin();
      return false;
    }
    stdin_termios_saved = true;
  } else if (errno != ENOTTY) {
    restore_stdin();
    return false;
  }

  saved_sigint_handler = signal(SIGINT, request_termination);
  if (saved_sigint_handler == SIG_ERR) {
    restore_stdin();
    return false;
  }
  saved_sigterm_handler = signal(SIGTERM, request_termination);
  if (saved_sigterm_handler == SIG_ERR) {
    signal(SIGINT, saved_sigint_handler);
    restore_stdin();
    return false;
  }
  termination_handlers_saved = true;
  if (!restore_at_exit_registered) {
    atexit(restore_stdin);
    restore_at_exit_registered = true;
  }

  return true;
}

bool activate_uart_mode(void) {
  clear_queued_input();

  if (debug_mode) {
    discard_terminal_input();
    if (!start_terminal_viewer()) {
      uart_mode = false;
      return false;
    }
    uart_mode = true;
    set_tui_uart_mode(true);
    nodelay(stdscr, TRUE);
    return true;
  }

  if (!prepare_stdin()) {
    uart_mode = false;
    return false;
  }
  uart_mode = true;
  return true;
}

void close_uart_mode(void) {
  if (debug_mode && uart_mode) {
    nodelay(stdscr, FALSE);
    set_tui_uart_mode(false);
  }
  restore_stdin();
  clear_queued_input();
  uart_mode = false;
}

static bool handle_input_byte(uint8_t byte) {
  if (byte == UART_MODE_ESCAPE) {
    close_uart_mode();
    return false;
  }
  append_queued_input(byte);
  return true;
}

static void read_debug_input(void) {
  uint8_t byte;
  if (read_terminal_input(&byte) && !handle_input_byte(byte)) {
    return;
  }

  int key;
  while ((key = getch()) != ERR) {
    if (key == UART_MODE_ESCAPE) {
      close_uart_mode();
      return;
    }
  }
}

static void read_stdin_input(void) {
  uint8_t byte;
  if (read(STDIN_FILENO, &byte, 1) != 1) {
    return;
  }
  handle_input_byte(byte);
}

void update_uart_mode(void) {
  if (!uart_mode) {
    return;
  }
  if (termination_signal != 0) {
    int signal_number = termination_signal;
    termination_signal = 0;
    close_uart_mode();
    raise(signal_number);
    return;
  }

  if (debug_mode) {
    read_debug_input();
  } else {
    read_stdin_input();
  }
  if (!uart_mode || queued_input_idx >= queued_input_len) {
    return;
  }

  if (uart_interrupt_trigger(queued_input[queued_input_idx])) {
    queued_input_idx++;
    if (queued_input_idx == queued_input_len) {
      clear_queued_input();
    }
  }
}

#include "../../include/uart_terminal.h"
#include "../../include/interrupt.h"
#include "../../include/parse/parse_args.h"
#include "../../include/terminal_view.h"
#include <errno.h>
#include <fcntl.h>
#include <ncurses.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#define UART_TERMINAL_ESCAPE 27
#define UART_RAW_TERMINAL_EXIT 29 // Uses Ctrl+] to leave raw input

static struct termios saved_stdin_termios;
static int saved_stdin_flags = 0;
static bool stdin_termios_saved = false;
static bool stdin_flags_saved = false;
static bool restore_at_exit_registered = false;
static bool termination_handlers_saved = false;
static void (*saved_sigint_handler)(int) = SIG_DFL;
static void (*saved_sigquit_handler)(int) = SIG_DFL;
static void (*saved_sigterm_handler)(int) = SIG_DFL;
static void (*saved_sigtstp_handler)(int) = SIG_DFL;
static volatile sig_atomic_t termination_signal = 0;
static uint8_t *queued_input = NULL;
static size_t queued_input_len = 0;
static size_t queued_input_idx = 0;
static bool terminal_active = false;
static bool raw_terminal_active = false;

static void clear_queued_input(void) {
  free(queued_input);
  queued_input = NULL;
  queued_input_len = 0;
  queued_input_idx = 0;
}

static void append_queued_input(uint8_t byte) {
  uint8_t *new_input = realloc(queued_input, queued_input_len + 1);
  if (new_input == NULL) {
    fprintf(stderr, "Error: Couldn't allocate UART terminal input buffer\n");
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
    signal(SIGQUIT, saved_sigquit_handler);
    signal(SIGTERM, saved_sigterm_handler);
    signal(SIGTSTP, saved_sigtstp_handler);
    termination_handlers_saved = false;
  }
}

static void request_termination(int signal_number) {
  termination_signal = signal_number;
}

static bool prepare_stdin(bool raw_input) {
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
    if (raw_input) {
      uart_termios.c_lflag &= ~(ISIG | IEXTEN);
    }
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
  saved_sigquit_handler = signal(SIGQUIT, request_termination);
  if (saved_sigquit_handler == SIG_ERR) {
    signal(SIGINT, saved_sigint_handler);
    restore_stdin();
    return false;
  }
  saved_sigterm_handler = signal(SIGTERM, request_termination);
  if (saved_sigterm_handler == SIG_ERR) {
    signal(SIGINT, saved_sigint_handler);
    signal(SIGQUIT, saved_sigquit_handler);
    restore_stdin();
    return false;
  }
  saved_sigtstp_handler = signal(SIGTSTP, request_termination);
  if (saved_sigtstp_handler == SIG_ERR) {
    signal(SIGINT, saved_sigint_handler);
    signal(SIGQUIT, saved_sigquit_handler);
    signal(SIGTERM, saved_sigterm_handler);
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

static void restore_debug_tui(void) {
  reset_prog_mode();
  clearok(stdscr, true);
}

static void show_debug_terminal(void) {
  fputs("\x1b[2J\x1b[H", stdout);
  replay_terminal_output();
}

bool activate_uart_terminal(bool raw_input) {
  if (terminal_active) {
    return true;
  }

  clear_queued_input();

  if (debug_mode) {
    def_prog_mode();
    endwin();
  }

  if (!prepare_stdin(raw_input)) {
    if (debug_mode) {
      restore_debug_tui();
    }
    return false;
  }

  terminal_active = true;
  raw_terminal_active = raw_input;
  if (debug_mode) {
    show_debug_terminal();
  }
  return true;
}

bool uart_terminal_is_active(void) { return terminal_active; }

void close_uart_terminal(void) {
  if (!terminal_active) {
    restore_stdin();
    clear_queued_input();
    return;
  }

  restore_stdin();
  clear_queued_input();
  terminal_active = false;
  raw_terminal_active = false;

  if (debug_mode) {
    restore_debug_tui();
  }
}

static bool handle_input_byte(uint8_t byte) {
  if (debug_mode &&
      ((!raw_terminal_active && byte == UART_TERMINAL_ESCAPE) ||
       (raw_terminal_active && byte == UART_RAW_TERMINAL_EXIT))) {
    close_uart_terminal();
    return false;
  }
  append_queued_input(byte);
  return true;
}

static ssize_t read_terminal_input(void) {
  uint8_t byte;
  ssize_t result = read(STDIN_FILENO, &byte, 1);
  if (result == 1) {
    handle_input_byte(byte);
  }
  return result;
}

void update_uart_terminal(void) {
  if (!terminal_active) {
    return;
  }
  if (termination_signal != 0) {
    int signal_number = termination_signal;
    termination_signal = 0;
    close_uart_terminal();
    raise(signal_number);
    return;
  }

  (void)read_terminal_input();
  if (!terminal_active || queued_input_idx >= queued_input_len) {
    return;
  }

  if (uart_interrupt_trigger(queued_input[queued_input_idx])) {
    queued_input_idx++;
    if (queued_input_idx == queued_input_len) {
      clear_queued_input();
    }
  }
}

void wait_for_uart_terminal_exit(void) {
  struct pollfd input = {.fd = STDIN_FILENO, .events = POLLIN};

  while (terminal_active) {
    if (termination_signal != 0) {
      update_uart_terminal();
      return;
    }
    input.revents = 0;
    if (poll(&input, 1, -1) < 0) {
      if (errno == EINTR) {
        continue;
      }
      close_uart_terminal();
      return;
    }

    ssize_t result = read_terminal_input();
    if (result == 1) {
      clear_queued_input();
      continue;
    }
    if (result == 0 && (input.revents & POLLHUP) != 0) {
      close_uart_terminal();
      return;
    }
    if (result < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
      close_uart_terminal();
      return;
    }
  }
}

#include "../include/debug.h"
#include "../include/assemble.h"
#include "../include/input_output.h"
#include "../include/interrupt.h"
#include "../include/log.h"
#include "../include/parse_args.h"
#include "../include/parse_instrs.h"
#include "../include/reti.h"
#include "../include/special_opts.h"
#include "../include/statemachine.h"
#include "../include/tui.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include <limits.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdint.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

uint8_t radius = 2;
static bool snapshot_available = false;
static pid_t snapshot_child_pid = -1;
static int snapshot_child_write_fd = -1;

static const char *SNAPSHOT_ROOT_DIR = "/tmp/reti_emulator";
static const char *SNAPSHOT_SRAM_PATH = "/tmp/reti_emulator/sram.bin";

char **gargv;

const Menu_Entry box_entries[] = {
    {"EPROM", EPROM_BOX},
    {"SRAM Codesegment", SRAM_C_BOX},
    {"SRAM Datasegment", SRAM_D_BOX},
    {"SRAM Stack", SRAM_S_BOX},
};

const uint8_t NUM_BOX_ENTRIES = sizeof(box_entries) / sizeof(box_entries[0]);

const Menu_Entry register_entries[] = {
    {"PC", PC},   {"IN1", IN1}, {"IN2", IN2}, {"ACC", ACC},         {"SP", SP},
    {"BAF", BAF}, {"CS", CS},   {"DS", DS},   {"Address", ADDRESS},
};

const Menu_Entry identifier_to_register_or_address[] = {
    {"PC", PC},   {"IN1", IN1}, {"IN2", IN2}, {"ACC", ACC},   {"SP", SP},
    {"BAF", BAF}, {"CS", CS},   {"DS", DS},   {"A", ADDRESS},
};

const char *register_or_address_to_identifier[] = {
    "PC", "IN1", "IN2", "ACC", "SP", "BAF", "CS", "DS", "Address"};

const uint8_t NUM_REGISTER_ENTRIES =
    sizeof(register_entries) / sizeof(register_entries[0]);

const uint8_t LINEWIDTH = 54;

WatchBox eprom_watchbox = {&eprom_box, PC, NULL};
WatchBox sram_c_watchbox = {&sram_c_box, PC, NULL};
WatchBox sram_d_watchbox = {&sram_d_box, DS, NULL};
WatchBox sram_s_watchbox = {&sram_s_box, SP, NULL};

Mnemonic_to_String opcode_to_mnemonic[] = {
    {ADDI, "ADDI"},     {SUBI, "SUBI"},       {MULTI, "MULTI"},
    {DIVI, "DIVI"},     {MODI, "MODI"},       {OPLUSI, "OPLUSI"},
    {ORI, "ORI"},       {ANDI, "ANDI"},       {ADDR, "ADD"},
    {SUBR, "SUB"},      {MULTR, "MULT"},      {DIVR, "DIV"},
    {MODR, "MOD"},      {OPLUSR, "OPLUS"},    {ORR, "OR"},
    {ANDR, "AND"},      {ADDM, "ADD"},        {SUBM, "SUB"},
    {MULTM, "MULT"},    {DIVM, "DIV"},        {MODM, "MOD"},
    {OPLUSM, "OPLUS"},  {ORM, "OR"},          {ANDM, "AND"},
    {LOAD, "LOAD"},     {LOADIN, "LOADIN"},   {LOADI, "LOADI"},
    {STORE, "STORE"},   {STOREIN, "STOREIN"}, {TSL, "TSL"},
    {MOVE, "MOVE"},
    {JUMPGT, "JUMP>"},  {JUMPEQ, "JUMP=="},   {JUMPEQ, "JUMP="},
    {JUMPGE, "JUMP>="}, {JUMPLT, "JUMP<"},    {JUMPNE, "JUMP!="},
    {JUMPNE, "JUMP<>"}, {JUMPLE, "JUMP<="},   {JUMP, "JUMP"},
    {INT, "INT"},       {RTI, "RTI"},         {NOP, "NOP"}};

char *copy_mnemonic_into_str(char *dest, const uint8_t opcode) {
  strcat(dest, opcode_to_mnemonic[opcode].name);
  return dest + strlen(dest);
}

char *copy_reg_into_str(char *dest, const uint8_t reg) {
  strcat(dest, " ");
  dest = strcat(dest, register_code_to_name[reg]);
  return dest + strlen(dest);
}

char *copy_im_into_str(char *dest, const uint32_t im) {
  strcpy(dest, " ");
  if (binary_mode) {
    sprintf(dest + 1, "%s", int_to_bin_str(im, 22));
  } else {
    sprintf(dest + 1, "%d", im);
  }
  return dest + strlen(dest);
}

char *assembly_to_str(Instruction *instr) {
  char *instr_str;
  if (binary_mode) {
    instr_str = malloc(39); // STOREIN ACC IN2 22bit\0
  } else {
    instr_str = malloc(25); // STOREIN ACC IN2 -2097152\0
  }
  instr_str[0] = '\0';
  char *dest = instr_str;
  for (size_t i = 0;
       i < sizeof(opcode_to_mnemonic) / sizeof(opcode_to_mnemonic[0]); i++) {
    if (opcode_to_mnemonic[i].value == instr->op) {
      dest = copy_mnemonic_into_str(dest, i);
      break;
    }
  }
  if ((ADDI <= instr->op && instr->op <= ANDI) ||
      (ADDM <= instr->op && instr->op <= ANDM)) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_im_into_str(dest, instr->opd2);
  } else if (ADDR <= instr->op && instr->op <= ANDR) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
  } else if (instr->op == LOAD || instr->op == STORE || instr->op == LOADI) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_im_into_str(dest, instr->opd2);
  } else if (instr->op == LOADIN || instr->op == STOREIN || instr->op == TSL) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
    dest = copy_im_into_str(dest, instr->opd3);
  } else if (instr->op == MOVE) {
    dest = copy_reg_into_str(dest, instr->opd1);
    dest = copy_reg_into_str(dest, instr->opd2);
  } else if ((JUMPGT <= instr->op && instr->op <= JUMP) || instr->op == INT) {
    dest = copy_im_into_str(dest, instr->opd1);
  } else if (instr->op == RTI || instr->op == NOP) {
  } else {
    fprintf(stderr, "Invalid instruction\n");
    exit(EXIT_FAILURE);
  }
  return instr_str;
}

char *mem_value_to_str(int32_t mem_content, bool is_unsigned) {
  char *instr_str = malloc(12); // -2147483649
  if (is_unsigned) {
    sprintf(instr_str, "%u", mem_content);
  } else {
    sprintf(instr_str, "%d", mem_content);
  }
  return instr_str;
}

char *mem_value_to_bin_str(uint32_t mem_content) {
  return int_to_bin_str(mem_content, 32);
}

char *reg_to_mem_pntr(uint64_t idx, MemType mem_type) {
  char *active_regs = "";
  bool at_least_one_reg = false;
  for (int i = 0; i < NUM_REGISTERS; i++) {
    uint32_t addr = read_array(regs, i, false);
    uint8_t addr_mem_type = addr >> 30;
    uint32_t addr_idx;
    if (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S) {
      addr_idx = addr & 0x7FFFFFFF;
    } else {
      addr_idx = addr & 0x3FFFFFFF;
    }
    if (((addr_mem_type == 0b11 &&
          (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S)) ||
         (addr_mem_type == 0b10 &&
          (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S)) ||
         (addr_mem_type == 0b01 && mem_type == UART) ||
         (addr_mem_type == 0b00 && mem_type == EPROM)) &&
        addr_idx == idx) {
      active_regs = proper_str_cat(active_regs, " ");
      active_regs = proper_str_cat(active_regs, register_code_to_name[i]);
      at_least_one_reg = true;
    }
  }
  if (at_least_one_reg) {
    return proper_str_cat("<-", active_regs);
  }
  return "";
}

void print_formatted_to_box(const char *format, Box *box, ...);
WatchBox *get_watchbox(BoxIdentifier box_identifier);
void assign_watchobject_to_box(WatchBox *watchbox, Register watchobject);

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

static void discard_snapshot_process(void) {
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
  char *sram_path = proper_str_cat(peripherals_dir, "/sram.bin");
  bool ok = fflush(sram) == 0 && fsync(fileno(sram)) == 0 &&
            copy_file_contents(sram_path, SNAPSHOT_SRAM_PATH);
  free(sram_path);
  return ok;
}

static bool reopen_snapshot_sram(void) {
  fclose(sram);
  sram = fopen(SNAPSHOT_SRAM_PATH, "r+b");
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
    return;
  }
}

// returns -1 on error, 0 in the restored child, 1 in the current process
static int create_snapshot(void) {
  mkdir(SNAPSHOT_ROOT_DIR, 0700);

  if (!copy_current_sram_to_snapshot()) {
    return -1;
  }

  discard_snapshot_process();

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

static void handle_watchobject_assignment(void) {
  BoxIdentifier box_identifier = display_popup_menu(box_entries, NUM_BOX_ENTRIES);
  WatchBox *watchbox = get_watchbox(box_identifier);

  if (box_identifier == CANCEL) {
    draw_tui();
    return;
  }

  Register watchobject =
      display_popup_menu(register_entries, NUM_REGISTER_ENTRIES);
  if (watchobject == CANCEL2) {
    draw_tui();
    return;
  }

  switch (box_identifier) {
  case EPROM_BOX:
  case SRAM_C_BOX:
  case SRAM_D_BOX:
  case SRAM_S_BOX:
    assign_watchobject_to_box(watchbox, watchobject);
    break;
  default:
    display_notification_box("Error", "Invalid box identifier");
    break;
  }
}

static Box *get_box_for_mem_type(MemType mem_type) {
  switch (mem_type) {
  case REGS:
    return &regs_box;
  case EPROM:
    return &eprom_box;
  case UART:
    return &uart_box;
  case SRAM_C:
    return &sram_c_box;
  case SRAM_D:
    return &sram_d_box;
  case SRAM_S:
    return &sram_s_box;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

static bool comment_matches_mem_type(Source_Comment *comment, MemType mem_type) {
  if (comment->target == COMMENT_TARGET_EPROM) {
    return mem_type == EPROM;
  }
  return mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S;
}

static bool mem_type_has_instruction_comments(MemType mem_type, uint64_t idx) {
  if (!collect_comments) {
    return false;
  }
  if (mem_type == EPROM) {
    return idx < num_instrs_start_prgrm;
  }
  return idx >= num_instrs_isrs && idx < num_instrs_isrs + num_instrs_prgrm &&
         (mem_type == SRAM_C || mem_type == SRAM_D || mem_type == SRAM_S);
}

static uint32_t wrapped_comment_rows(Box *box, const char *text) {
  int max_comment_len = max(0, box->width - 2);
  if (max_comment_len == 0) {
    return 0;
  }

  size_t text_len = strlen(text);
  return max(1, (text_len + max_comment_len - 1) / max_comment_len);
}

static uint32_t count_comments_for_instruction(MemType mem_type, uint64_t idx,
                                               bool before_instruction) {
  if (!mem_type_has_instruction_comments(mem_type, idx)) {
    return 0;
  }

  uint32_t count = 0;
  Box *box = get_box_for_mem_type(mem_type);
  for (uint32_t i = 0; i < num_source_comments; i++) {
    Source_Comment *comment = &source_comments[i];
    if (comment_matches_mem_type(comment, mem_type) &&
        comment->anchor_idx == idx &&
        comment->display_before_instr == before_instruction) {
      count += wrapped_comment_rows(box, comment->text);
    }
  }
  return count;
}

static uint32_t rendered_rows_for_idx(MemType mem_type, uint64_t idx) {
  return 1 + count_comments_for_instruction(mem_type, idx, true) +
         count_comments_for_instruction(mem_type, idx, false);
}

static uint64_t max_idx_for_mem_type(MemType mem_type) {
  switch (mem_type) {
  case EPROM:
    return EPROM_SIZE - 1;
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    return sram_size - 1;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

static void determine_visible_range(MemType mem_type, uint64_t watch_idx,
                                    uint16_t max_rows, uint64_t *start,
                                    uint64_t *end) {
  uint64_t min_idx = 0;
  uint64_t max_idx = max_idx_for_mem_type(mem_type);

  *start = watch_idx;
  *end = watch_idx;

  uint32_t rows_before_watch =
      count_comments_for_instruction(mem_type, watch_idx, true);
  uint32_t rows_after_watch =
      count_comments_for_instruction(mem_type, watch_idx, false);
  uint32_t used_rows = rows_before_watch + 1 + rows_after_watch;

  while (used_rows < max_rows && (*start > min_idx || *end < max_idx)) {
    bool can_expand_up = *start > min_idx;
    bool can_expand_down = *end < max_idx;

    if (can_expand_up &&
        (!can_expand_down || rows_before_watch <= rows_after_watch)) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      if (used_rows + added_rows > max_rows) {
        can_expand_up = false;
      } else {
        (*start)--;
        rows_before_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    if (can_expand_down) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *end + 1);
      if (used_rows + added_rows > max_rows) {
        can_expand_down = false;
      } else {
        (*end)++;
        rows_after_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    if (can_expand_up) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      if (used_rows + added_rows <= max_rows) {
        (*start)--;
        rows_before_watch += added_rows;
        used_rows += added_rows;
        continue;
      }
    }

    break;
  }

  if (used_rows < max_rows) {
    uint32_t up_overflow = UINT32_MAX;
    uint32_t down_overflow = UINT32_MAX;

    if (*start > min_idx) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *start - 1);
      up_overflow = used_rows + added_rows - max_rows;
    }
    if (*end < max_idx) {
      uint32_t added_rows = rendered_rows_for_idx(mem_type, *end + 1);
      down_overflow = used_rows + added_rows - max_rows;
    }

    if (up_overflow == UINT32_MAX && down_overflow == UINT32_MAX) {
      return;
    }
    if (up_overflow <= down_overflow) {
      (*start)--;
    } else {
      (*end)++;
    }
  }
}

static void print_comments_for_instruction(MemType mem_type, uint64_t idx,
                                           bool before_instruction) {
  if (!collect_comments) {
    return;
  }

  Box *box = get_box_for_mem_type(mem_type);
  int max_comment_len = max(0, box->width - 2);
  if (max_comment_len == 0) {
    return;
  }
  for (uint32_t i = 0; i < num_source_comments; i++) {
    Source_Comment *comment = &source_comments[i];
    if (!comment_matches_mem_type(comment, mem_type) ||
        comment->anchor_idx != idx ||
        comment->display_before_instr != before_instruction) {
      continue;
    }
    size_t text_len = strlen(comment->text);
    size_t offset = 0;
    do {
      int chunk_len = min((int64_t)(text_len - offset), max_comment_len);
      int formatted_len = snprintf(NULL, 0, "%-*.*s\n", max_comment_len,
                                   chunk_len, comment->text + offset);
      char *formatted_comment = malloc(formatted_len + 1);
      snprintf(formatted_comment, formatted_len + 1, "%-*.*s\n",
               max_comment_len, chunk_len, comment->text + offset);
      write_text_into_box_with_attr(box, formatted_comment,
                                    COLOR_PAIR(COMMENT_COLOR_PAIR));
      free(formatted_comment);
      offset += chunk_len;
    } while (offset < text_len);
  }
}

void print_formatted_to_box(const char *format, Box *box, ...) {
  va_list args;
  va_start(args, box);

  char *final_str = create_formatted_str(format, args);
  write_text_into_box(box, final_str);

  va_end(args);
}

// TODO:: split zwischen mem content und assembly instrs
// TODO:: Unit test dafür und die ganzen idx Funktionen
void print_mem_content_with_idx(uint64_t idx, uint32_t mem_content,
                                bool are_unsigned, bool are_instrs,
                                MemType mem_type) {
  char idx_str[20];
  switch (mem_type) {
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    snprintf(
        idx_str, sizeof(idx_str),
        proper_str_cat(
            proper_str_cat("%0", num_digits_for_idx_str(sram_size - 1)), "zu"),
        idx);
    break;
  case EPROM:
    snprintf(idx_str, sizeof(idx_str),
             proper_str_cat(proper_str_cat("%0", num_digits_for_idx_str(
                                                     num_instrs_start_prgrm)),
                            "zu"),
             idx);
    break;
  case UART:
    snprintf(idx_str, sizeof(idx_str),
             proper_str_cat(proper_str_cat("%0", num_digits_for_idx_str(
                                                     NUM_UART_ADDRESSES)),
                            "zu"),
             idx);
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
  const char *mem_content_str;
  if (are_instrs) {
    mem_content_str = assembly_to_str(machine_to_assembly(mem_content));
  } else {
    if (binary_mode) {
      mem_content_str = mem_value_to_bin_str(mem_content);
    } else {
      mem_content_str = mem_value_to_str(mem_content, are_unsigned);
    }
  }

  char *reg_to_mem_pntr_str = reg_to_mem_pntr(idx, mem_type);
  Box *box = get_box_for_mem_type(mem_type);

  print_formatted_to_box("%s: %s%s\n", box, idx_str, mem_content_str,
                         reg_to_mem_pntr_str);
}

void print_reg_content_with_reg(uint8_t reg_idx, uint32_t mem_content) {
  char reg_str[4];
  snprintf(reg_str, sizeof(reg_str), "%3s", register_code_to_name[reg_idx]);
  const char *mem_content_str_unsigned;
  if (binary_mode) {
    mem_content_str_unsigned = mem_value_to_bin_str(mem_content);
  } else {
    mem_content_str_unsigned = mem_value_to_str(mem_content, true);
  }
  const char *mem_content_str_signed = mem_value_to_str(mem_content, false);

  print_formatted_to_box("%s: %s (%s)\n", &regs_box, reg_str,
                         mem_content_str_unsigned, mem_content_str_signed);
}

void print_array_with_idcs(MemType mem_type, uint8_t length, bool are_instrs) {
  print_array_with_idcs_from_to(mem_type, 0, length - 1, are_instrs);
}

void print_array_with_idcs_from_to(MemType mem_type, uint64_t start,
                                   uint64_t end, bool are_instrs) {
  switch (mem_type) {
  case REGS:
    for (uint8_t i = start; i <= end; i++) {
      print_reg_content_with_reg(i, ((uint32_t *)regs)[i]);
    }
    break;
  case EPROM:
    for (uint16_t i = start; i <= end; i++) {
      if (are_instrs) {
        print_comments_for_instruction(EPROM, i, true);
      }
      if (i < num_instrs_start_prgrm) {
        print_mem_content_with_idx(i, ((uint32_t *)eprom)[i], false, are_instrs,
                                   EPROM);
      } else {
        print_mem_content_with_idx(i, 0, false, false, EPROM);
      }
      if (are_instrs) {
        print_comments_for_instruction(EPROM, i, false);
      }
    }
    break;
  case UART:
    for (uint8_t i = start; i <= end; i++) {
      print_mem_content_with_idx(i, ((uint8_t *)uart)[i], false, are_instrs,
                                 UART);
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

void print_file_with_idcs(MemType mem_type, uint64_t start, uint64_t end,
                          bool are_unsigned, bool are_instrs) {
  switch (mem_type) {
  case SRAM_C:
  case SRAM_D:
  case SRAM_S:
    for (uint64_t i = start; i <= end; i++) {
      if (are_instrs) {
        print_comments_for_instruction(mem_type, i, true);
      }
      print_mem_content_with_idx(i, read_file(sram, i), are_unsigned,
                                 are_instrs, mem_type);
      if (are_instrs) {
        print_comments_for_instruction(mem_type, i, false);
      }
    }
    break;
  default:
    fprintf(stderr, "Error: Invalid memory type\n");
    exit(EXIT_FAILURE);
  }
}

uint64_t determine_watchobject_value(WatchBox *watchbox) {
  if (watchbox->watchobject != ADDRESS) {
    return read_array(regs, watchbox->watchobject, false);
  }

  if (watchbox->watchobject_addr == NULL) {
    display_notification_box("Error", "No address configured for this box");
    // shouldn't be possible
    return UINT64_MAX;
  }

  char *endptr;
  uint64_t watchobject_val = strtol(watchbox->watchobject_addr, &endptr, 10);
  if (*endptr != '\0') {
    const char *str = "Error: Invalid register or number: ";
    const char *str2 = proper_str_cat(str, watchbox->watchobject_addr);

    display_notification_box("Error", str2);
    // this value will never be reached
    return UINT64_MAX;
  } else if (watchobject_val < 0 &&
             watchobject_val >
                 UINT64_MAX) { // this should never happen, because the input
                               // already can't be higher than this
    display_notification_box(
        "Error", "Error: Number out of range, must be between 0 and "
                 "18446744073709551615");
    // this value will never be reached
    return UINT64_MAX;
  }

  return watchobject_val;
}

void print_eprom_watchobject(uint64_t eprom_watchobject) {
  if (eprom_watchobject & 0xC0000000) {
    return;
  }

  uint64_t start;
  uint64_t end;
  determine_visible_range(EPROM, eprom_watchobject, eprom_box.height - 2, &start,
                          &end);

  if (start < num_instrs_start_prgrm) {
    print_array_with_idcs_from_to(EPROM, start,
                                  min(end, num_instrs_start_prgrm - 1), true);
  }
  if (end >= num_instrs_start_prgrm) {
    print_array_with_idcs_from_to(EPROM, max(num_instrs_start_prgrm, start), end,
                                  false);
  }
}

void print_sram_watchobject(uint64_t sram_watchobject_x, MemType mem_type) {
  if (!(sram_watchobject_x & 0x80000000)) {
    return;
  }

  sram_watchobject_x = sram_watchobject_x & 0x7FFFFFFF;
  uint64_t start;
  uint64_t end;
  uint64_t instruction_start = ivt_max_idx == (uint32_t)-1 ? 0 : ivt_max_idx + 1;
  uint64_t instruction_end = num_instrs_isrs + num_instrs_prgrm - 1;
  determine_visible_range(mem_type, sram_watchobject_x, sram_c_box.height - 2,
                          &start, &end);

  if (ivt_max_idx != -1 && start <= ivt_max_idx) {
    print_file_with_idcs(mem_type, start, min(end, ivt_max_idx), true, false);
  }
  if (end >= instruction_start && start <= instruction_end) {
    print_file_with_idcs(mem_type, max(instruction_start, start),
                         min(end, instruction_end), false, true);
  }
  if (end >= num_instrs_isrs + num_instrs_prgrm) {
    print_file_with_idcs(mem_type,
                         max((uint64_t)(num_instrs_isrs + num_instrs_prgrm),
                             start),
                         end, ds_vals_unsigned, false);
  }
}

void print_uart_meta_data() {
  print_formatted_to_box("Current send data: %s\n", &uart_box,
                         current_send_data ? current_send_data : "");
  print_formatted_to_box("All send data: %s\n", &uart_box,
                         all_send_data ? all_send_data : "");
  print_formatted_to_box("Waiting time sending: ", &uart_box);
  print_formatted_to_box("%d\n", &uart_box, sending_waiting_time);
  print_formatted_to_box("Waiting time receiving: ", &uart_box);
  print_formatted_to_box("%d\n", &uart_box, receiving_waiting_time);
  if (receiving_waiting_time > 0) {
    print_formatted_to_box("Current input: %u\n", &uart_box, received_num_part);
  } else {
    print_formatted_to_box("Current input:\n", &uart_box);
  }
  print_formatted_to_box("Remaining input: ", &uart_box);
  if (read_metadata && input_idx < input_len) {
    for (uint8_t i = input_idx; i < input_len; i++) {
      if (i == input_idx && (int8_t)received_num_idx >= 0) {
        print_formatted_to_box("%d(", &uart_box, received_num);
        for (uint8_t j = received_num_idx; j != 0; j--) {
          uint8_t received_num_part =
              (received_num & (0xFF << (j * 8))) >> (j * 8);
          print_formatted_to_box("%u ", &uart_box, received_num_part);
        }
        uint8_t received_num_part = received_num & 0xFF;
        print_formatted_to_box("%u) ", &uart_box, received_num_part);
      } else {
        print_formatted_to_box("%d ", &uart_box, uart_input[i]);
      }
    }
  } else {
    if ((int8_t)received_num_idx >= 0) {
      print_formatted_to_box("%d(", &uart_box, received_num);
      for (uint8_t j = received_num_idx; j != 0; j--) {
        uint8_t received_num_part =
            (received_num & (0xFF << (j * 8))) >> (j * 8);
        print_formatted_to_box("%u ", &uart_box, received_num_part);
      }
      uint8_t received_num_part = received_num & 0xFF;
      print_formatted_to_box("%u)", &uart_box, received_num_part);
    }
  }
  print_formatted_to_box("\n", &uart_box);
}

WatchBox *get_watchbox(BoxIdentifier box_identifier) {
  switch (box_identifier) {
  case EPROM_BOX:
    return &eprom_watchbox;
  case SRAM_C_BOX:
    return &sram_c_watchbox;
  case SRAM_D_BOX:
    return &sram_d_watchbox;
  case SRAM_S_BOX:
    return &sram_s_watchbox;
  default:
    return NULL;
  }
}

char *ask_watchobject_addr(void) {
  char *watchobject_addr = malloc(MAX_CHARS_WATCHOBJECT + 1);
  display_input_box(watchobject_addr, "Enter an address:",
                    MAX_CHARS_WATCHOBJECT);
  return watchobject_addr;
}

void assign_watchobject_to_box(WatchBox *watchbox, Register watchobject) {
  Register previous_watchobject = watchbox->watchobject;
  char *previous_addr = watchbox->watchobject_addr;

  if (watchobject == ADDRESS) {
    watchbox->watchobject_addr = ask_watchobject_addr();
  }

  watchbox->watchobject = watchobject;
  if (draw_tui()) {
    if (watchobject == ADDRESS) {
      free(previous_addr);
    }
    return;
  }

  watchbox->watchobject = previous_watchobject;
  if (watchobject == ADDRESS) {
    free(watchbox->watchobject_addr);
    watchbox->watchobject_addr = previous_addr;
  }
}

void evaluate_keyboard_input(void) {
  char key;
  while (true) {
    char ch = getchar();
    if (ch == EOF) {
      continue;
    }
    key = (char)ch;
    if (key == 'n') {
      return;
    } else if (key == 'c') {
      update_state(CONTINUE);
      return;
    } else if (key == 'r') {
      discard_snapshot_process();
      finalize();
      execvp(gargv[0], gargv);
    } else if (key == 's') {
      update_state(STEP_INTO_ACTION);
      bool success = out.retbool1;
      if (success) {
        return;
      }
      continue;
    } else if (key == 'f') {
      update_state(FINALIZE);
      bool success = out.retbool1;
      if (success) {
        return;
      }
      continue;
    } else if (key == 't') {
      bool success = keypress_interrupt_trigger();
      if (success) {
        return;
      }
    } else if (key == 'a') {
      handle_watchobject_assignment();
    } else if (key == 'o') {
      cycle_info_box_page();
      draw_tui();
      continue;
    } else if (key == 'S') {
      int snapshot_result = create_snapshot();
      if (snapshot_result == 1) {
        display_notification_box("Snapshot",
                                 "Saved process state to /tmp/reti_emulator");
      } else if (snapshot_result == 0) {
        draw_tui();
        continue;
      } else {
        display_notification_box("Snapshot Error", "Snapshot failed");
      }
      draw_tui();
      continue;
    } else if (key == 'R') {
      pid_t restored_pid;
      if (!snapshot_available) {
        display_notification_box("Restore Error",
                                 "No snapshot available yet");
        draw_tui();
        continue;
      }
      if (!restore_snapshot(&restored_pid)) {
        display_notification_box("Restore Error", "Restore failed");
        draw_tui();
        continue;
      }
      waitpid(restored_pid, NULL, 0);
      _exit(EXIT_SUCCESS);
    } else if (key == 'e') {
      if (cycle_keypress_interrupt_action_isr()) {
        draw_tui();
      }
      continue;
    } else if (key == 'D') {
      debug_activated = !debug_activated;
    } else if (key == 'q') {
      discard_snapshot_process();
      finalize();
      exit(EXIT_SUCCESS);
    }
  }
}

void wait_for_tui_quit(void) {
  set_tui_halted_mode(true);

  while (true) {
    update_term_and_box_sizes();
    draw_tui();

    char ch = getchar();
    if (ch == EOF) {
      continue;
    }

    switch ((char)ch) {
    case 'a':
      handle_watchobject_assignment();
      break;
    case 'o':
      cycle_info_box_page();
      draw_tui();
      continue;
    case 'S':
      {
      int snapshot_result = create_snapshot();
      if (snapshot_result == 1) {
        display_notification_box("Snapshot",
                                 "Saved process state to /tmp/reti_emulator");
      } else if (snapshot_result == 0) {
        draw_tui();
        continue;
      } else {
        display_notification_box("Snapshot Error", "Snapshot failed");
      }
      draw_tui();
      continue;
      }
    case 'R':
      {
      pid_t restored_pid;
      if (!snapshot_available) {
        display_notification_box("Restore Error",
                                 "No snapshot available yet");
        draw_tui();
        continue;
      }
      if (!restore_snapshot(&restored_pid)) {
        display_notification_box("Restore Error", "Restore failed");
        draw_tui();
        continue;
      }
      waitpid(restored_pid, NULL, 0);
      _exit(EXIT_SUCCESS);
      }
    case 'q':
      discard_snapshot_process();
      set_tui_halted_mode(false);
      return;
    default:
      break;
    }
  }
}

void handle_heading(bool simple_debug_tui, Box *box, char *format_str,
                    const char *watchobject, uint64_t watchobject_int) {
  if (simple_debug_tui) {
    box->title = malloc(strlen(format_str) + 1);
    strcpy(box->title, format_str);
  } else {
    uint8_t len_title =
        snprintf(NULL, 0, format_str, watchobject, watchobject_int) + 1;
    box->title = malloc(len_title);
    snprintf(box->title, len_title, format_str, watchobject, watchobject_int);
  }
}

bool draw_tui(void) {
  uint64_t eprom_watchobject_int =
      determine_watchobject_value(&eprom_watchbox);
  uint64_t sram_watchobject_cs_int =
      determine_watchobject_value(&sram_c_watchbox);
  uint64_t sram_watchobject_ds_int =
      determine_watchobject_value(&sram_d_watchbox);
  uint64_t sram_watchobject_stack_int =
      determine_watchobject_value(&sram_s_watchbox);
  if (eprom_watchobject_int == UINT64_MAX ||
      sram_watchobject_cs_int == UINT64_MAX ||
      sram_watchobject_ds_int == UINT64_MAX ||
      sram_watchobject_stack_int == UINT64_MAX) {
    return false;
  }

  for (int i = 0; i < NUM_BOXES; i++) {
    wclear(boxes[i]->win);
    reset_box_line(boxes[i]);
    if (extended_features) {
      make_unneccessary_spaces_visible(boxes[i]);
    }
  }

  handle_heading(true, &regs_box, "Registers", "", 0);
  print_array_with_idcs(REGS, NUM_REGISTERS, false);

  handle_heading(false, &eprom_box, "EPROM: %s (%lu)",
                 register_or_address_to_identifier[eprom_watchbox.watchobject],
                 eprom_watchobject_int);
  print_eprom_watchobject(eprom_watchobject_int);

  handle_heading(true, &uart_box, "UART", "", 0);
  print_array_with_idcs(UART, NUM_UART_ADDRESSES, false);
  print_uart_meta_data();

  // the user shouldn't have to calculate the absolute address for the sram
  sram_watchobject_cs_int =
      sram_watchobject_cs_int +
      ((sram_c_watchbox.watchobject == ADDRESS) ? (uint32_t)(1 << 31) : 0);
  sram_watchobject_ds_int =
      sram_watchobject_ds_int +
      (uint64_t)((sram_d_watchbox.watchobject == ADDRESS) ? (uint32_t)(1 << 31)
                                                          : 0);
  sram_watchobject_stack_int =
      sram_watchobject_stack_int +
      (uint64_t)((sram_s_watchbox.watchobject == ADDRESS)
                     ? (uint32_t)(1 << 31)
                     : 0);

  handle_heading(false, &sram_c_box, "SRAM Codesegment: %s (%lu)",
                 register_or_address_to_identifier[sram_c_watchbox.watchobject],
                 sram_watchobject_cs_int);
  print_sram_watchobject(sram_watchobject_cs_int, SRAM_C);

  handle_heading(false, &sram_d_box, "SRAM Datasegment: %s (%lu)",
                 register_or_address_to_identifier[sram_d_watchbox.watchobject],
                 sram_watchobject_ds_int);
  print_sram_watchobject(sram_watchobject_ds_int, SRAM_D);

  handle_heading(false, &sram_s_box, "SRAM Stack: %s (%lu)",
                 register_or_address_to_identifier[sram_s_watchbox.watchobject],
                 sram_watchobject_stack_int);
  print_sram_watchobject(sram_watchobject_stack_int, SRAM_S);

  draw_boxes();

  return true;
}

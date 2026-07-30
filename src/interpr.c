#include "../include/interpr.h"
#include "../include/assemble.h"
#include "../include/datastructures.h"
#include "../include/core_debug.h"
#include "../include/error.h"
#include "../include/exception.h"
#include "../include/interpr.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/log.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include "../include/uart.h"
#include "../include/uart_terminal.h"
#include "../include/utils.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>

static bool write_instruction_register(uint8_t reg, uint32_t result) {
  uint32_t old_value = read_array(regs, reg, false);
  if (reg == SP && result < old_value &&
      stack_pointer_would_overflow(result)) {
    trigger_cpu_exception(CPU_EXCEPTION_STACK_OVERFLOW);
    return false;
  }

  write_array(regs, reg, result, false);
  return true;
}

void setup_interrupt(uint32_t isr) {
  write_array(regs, SP, read_array(regs, SP, false) - 1, false);
  write_storage(read_array(regs, SP, false) + 1, read_array(regs, PC, false));
  write_array(regs, PC, read_storage_sram_constant_fill(isr) | SRAM_CONST << 30,
              false);
}

void return_from_interrupt() {
  write_array(regs, PC, read_storage(read_array(regs, SP, false) + 1), false);
  write_array(regs, SP, read_array(regs, SP, false) + 1, false);
}

// TODO: Problem, dass immediates sign extended werden, aber bitweise xor, and
// und or auf das nicht sign extendete mit 0en drangefügt angewandt werden
// TODO: Alernative Lösung ohne sign extension mit:
// typedef struct {
//     uint32_t value : 22;
// } uint22_t;
void interpr_instr(Instruction *assembly_instr) {
  switch (assembly_instr->op) {
  // TODO: Tobias ADD PC 0 ist das gleiche wie JUMP 0, was ist damit?
  case ADDI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) +
                (int32_t)assembly_instr->opd2)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) -
                (int32_t)assembly_instr->opd2)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) *
                (int32_t)assembly_instr->opd2)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVI:
    if (assembly_instr->opd2 == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int64_t)(int32_t)read_array(
                regs, assembly_instr->opd1, false) /
                (int32_t)assembly_instr->opd2)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MODI:
    if (assembly_instr->opd2 == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                (int32_t)assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case OPLUSI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) ^
                (assembly_instr->opd2 & IMMEDIATE_MASK))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) |
                (assembly_instr->opd2 & IMMEDIATE_MASK))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDI:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) &
                (assembly_instr->opd2 & IMMEDIATE_MASK))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ADDR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) +
                (int32_t)read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) -
                (int32_t)read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) *
                (int32_t)read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVR: {
    int32_t divisor = read_array(regs, assembly_instr->opd2, false);
    if (divisor == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int64_t)(int32_t)read_array(
                regs, assembly_instr->opd1, false) /
                divisor)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
  }
    break;
  case MODR: {
    int32_t divisor = read_array(regs, assembly_instr->opd2, false);
    if (divisor == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                divisor))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
  }
    break;
  case OPLUSR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) ^
                read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) |
                read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDR:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) &
                read_array(regs, assembly_instr->opd2, false))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ADDM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) +
                (int32_t)read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) -
                (int32_t)read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int32_t)read_array(regs, assembly_instr->opd1, false) *
                (int32_t)read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVM: {
    int32_t divisor = read_storage_ds_fill(assembly_instr->opd2);
    if (divisor == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            (int64_t)(int32_t)read_array(
                regs, assembly_instr->opd1, false) /
                divisor)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
  }
    break;
  case MODM: {
    int32_t divisor = read_storage_ds_fill(assembly_instr->opd2);
    if (divisor == 0) {
      trigger_cpu_exception(CPU_EXCEPTION_DIVIDE_BY_ZERO);
      return;
    }
    if (!write_instruction_register(
            assembly_instr->opd1,
            mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                divisor))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
  }
    break;
  case OPLUSM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) ^
                read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) |
                read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDM:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_array(regs, assembly_instr->opd1, false) &
                read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case LOAD:
    if (!write_instruction_register(
            assembly_instr->opd1,
            read_storage_ds_fill(assembly_instr->opd2))) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case LOADIN:
    if (!write_instruction_register(
            assembly_instr->opd2,
            read_storage(read_array(regs, assembly_instr->opd1, false) +
                         (int32_t)assembly_instr->opd3))) {
      return;
    }
    if (assembly_instr->opd2 == PC) {
      // TODO: Testcases für genau das
      goto no_pc_increase;
    }
    break;
  case LOADI:
    // In case i is not allowed to be signed need mask
    // write_array(regs, assembly_instr->opd1,
    //             assembly_instr->opd2 & IMMEDIATE_MASK, false);
    if (!write_instruction_register(assembly_instr->opd1,
                                    assembly_instr->opd2)) {
      return;
    }
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case STORE:
    write_storage_ds_fill(assembly_instr->opd2,
                          read_array(regs, assembly_instr->opd1, false));
    break;
  case STOREIN:
    write_storage(read_array(regs, assembly_instr->opd1, false) +
                      (int32_t)assembly_instr->opd3,
                  read_array(regs, assembly_instr->opd2, false));
    break;
  case TSL: {
    uint32_t addr = read_array(regs, assembly_instr->opd1, false) +
                    (int32_t)assembly_instr->opd3;
    uint32_t source_value = read_storage(addr);
    if (!write_instruction_register(assembly_instr->opd2, source_value)) {
      return;
    }
    write_storage(addr, 1);
    if (assembly_instr->opd2 == PC) {
      goto no_pc_increase;
    }
  } break;
  case MOVE:
    if (!write_instruction_register(
            assembly_instr->opd2,
            read_array(regs, assembly_instr->opd1, false))) {
      return;
    }
    if (assembly_instr->opd2 == PC) {
      goto no_pc_increase;
    }
    break;
  case NOP:
    break;
  case INT:
    in.arg8 = assembly_instr->opd1;
    update_state(SOFTWARE_INTERRUPT);
    goto no_pc_increase;
  case RTI:
    update_state(RETURN_FROM_INTERRUPT);
    break;
  case JUMPGT:
    if ((int32_t)read_array(regs, ACC, false) > 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMPEQ:
    if (read_array(regs, ACC, false) == 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMPGE:
    if ((int32_t)read_array(regs, ACC, false) >= 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMPLT:
    if ((int32_t)read_array(regs, ACC, false) < 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMPNE:
    if (read_array(regs, ACC, false) != 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMPLE:
    if ((int32_t)read_array(regs, ACC, false) <= 0) {
      write_array(regs, PC,
                  read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                  false);
      goto no_pc_increase;
    }
    break;
  case JUMP:
    write_array(regs, PC,
                read_array(regs, PC, false) + (int32_t)assembly_instr->opd1,
                false);
    goto no_pc_increase;
  default:
    trigger_cpu_exception(CPU_EXCEPTION_ILLEGAL_INSTRUCTION);
    return;
  }
  write_array(regs, PC, read_array(regs, PC, false) + 1, false);
no_pc_increase:;
}

void interpr_prgrm() {
  sync_source_debug_state();
  while (true) {
    sync_source_debug_state();
    bool terminal_was_active = uart_terminal_is_active();
    update_uart_terminal();
    bool terminal_closed =
        debug_mode && terminal_was_active && !uart_terminal_is_active();
    if (!terminal_closed) {
      poll_running_debug_action();
    }
    if (visibility_condition) {
      update_term_and_box_sizes();
      draw_tui();
      evaluate_keyboard_input();
    } else if (terminal_closed) {
      update_term_and_box_sizes();
      draw_tui();
    }
    if (timer_interrupt_check()) {
      continue;
    };

    uint32_t machine_instr = read_storage(read_array(regs, PC, false));
    if (!machine_word_is_valid_instruction(machine_instr)) {
      trigger_cpu_exception(CPU_EXCEPTION_ILLEGAL_INSTRUCTION);
      update_uart();
      continue;
    }
    Instruction *assembly_instr = machine_to_assembly(machine_instr);

    if (assembly_instr->op == JUMP && assembly_instr->opd1 == 0) {
      free(assembly_instr);
      break;
    } else if (assembly_instr->op == INT && assembly_instr->opd1 == 3) {
      // Keeps hidden interrupt execution from entering a paused state
      if (!debug_mode ||
          (!uart_terminal_is_active() && isr_finished && isr_step_into)) {
        update_state(BREAKPOINT_ENCOUNTERED);
        stop_continuous_execution();
      }
      write_array(regs, PC, read_array(regs, PC, false) + 1, false);
    } else {
      interpr_instr(assembly_instr);
      free(assembly_instr);
    }

    update_uart();
  }

  stop_continuous_execution();
}

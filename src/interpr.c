#include "../include/interpr.h"
#include "../include/assemble.h"
#include "../include/datastructures.h"
#include "../include/debug.h"
#include "../include/error.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/tui.h"
#include "../include/uart.h"
#include "../include/utils.h"
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>

bool is_hardware_interrupt = false;

uint8_t current_isr;

void setup_interrupt(uint32_t ivt_table_addr) {
  write_array(regs, SP, read_array(regs, SP, false) - 1, false);
  write_storage(read_array(regs, SP, false) + 1, read_array(regs, PC, false));
  // TODO: Tobias, wird mit DS ausgefüllt?
  // write_array(regs, PC, read_storage_ds_fill(assembly_instr->opd1), false);
  write_array(regs, PC, read_storage_sram_constant_fill(ivt_table_addr), false);
}

static void return_from_interrupt() {
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
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) +
                    (int32_t)assembly_instr->opd2,
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBI:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) -
                    (int32_t)assembly_instr->opd2,
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTI:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) *
                    (int32_t)assembly_instr->opd2,
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVI:
    if (assembly_instr->opd2 == 0) {
      display_error_message("DivisionByZeroError", "Dividing by Immediate 0",
                            NULL, Idx);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    write_array(regs, assembly_instr->opd1,
                (int32_t)(read_array(regs, assembly_instr->opd1, false)) /
                    (int32_t)assembly_instr->opd2,
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MODI:
    write_array(regs, assembly_instr->opd1,
                mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                    (int32_t)assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case OPLUSI:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) ^
                    (assembly_instr->opd2 & IMMEDIATE_MASK),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORI:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) |
                    (assembly_instr->opd2 & IMMEDIATE_MASK),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDI:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) &
                    (assembly_instr->opd2 & IMMEDIATE_MASK),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ADDR:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) +
                    (int32_t)read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBR:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) -
                    (int32_t)read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTR:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) *
                    (int32_t)read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVR:
    if (read_array(regs, assembly_instr->opd2, false) == 0) {
      display_error_message("DivisionByZeroError",
                            "Dividing by content of Register %s which is 0",
                            register_code_to_name[assembly_instr->opd2], Idx);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) /
                    (int32_t)read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MODR:
    write_array(regs, assembly_instr->opd1,
                mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                    (int32_t)read_array(regs, assembly_instr->opd2, false)),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case OPLUSR:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) ^
                    read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORR:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) |
                    read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDR:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) &
                    read_array(regs, assembly_instr->opd2, false),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ADDM:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) +
                    (int32_t)read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case SUBM:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) -
                    (int32_t)read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MULTM:
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) *
                    (int32_t)read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case DIVM:
    if (read_storage_ds_fill(assembly_instr->opd2) == 0) {
      char *addr_str = malloc(MAX_DIGITS_ADDR_DEC);
      sprintf(addr_str, "%d", assembly_instr->opd2);
      display_error_message(
          "DivisionByZeroError",
          "Dividing by memory content at address %s which is 0",
          (const char *)addr_str, Idx);
      exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
    }
    write_array(regs, assembly_instr->opd1,
                (int32_t)read_array(regs, assembly_instr->opd1, false) /
                    (int32_t)read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case MODM:
    write_array(regs, assembly_instr->opd1,
                mod((int32_t)read_array(regs, assembly_instr->opd1, false),
                    (int32_t)read_storage_ds_fill(assembly_instr->opd2)),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case OPLUSM:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) ^
                    read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ORM:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) |
                    read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case ANDM:
    write_array(regs, assembly_instr->opd1,
                read_array(regs, assembly_instr->opd1, false) &
                    read_storage_ds_fill(assembly_instr->opd2),
                false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case LOAD:
    write_array(regs, assembly_instr->opd1,
                read_storage_ds_fill(assembly_instr->opd2), false);
    if (assembly_instr->opd1 == PC) {
      goto no_pc_increase;
    }
    break;
  case LOADIN:
    write_array(regs, assembly_instr->opd2,
                read_storage(read_array(regs, assembly_instr->opd1, false) +
                             (int32_t)assembly_instr->opd3),
                false);
    if (assembly_instr->opd2 == PC) {
      // TODO: Testcases für genau das
      goto no_pc_increase;
    }
    break;
  case LOADI:
    // In case i is not allowed to be signed need mask
    // write_array(regs, assembly_instr->opd1,
    //             assembly_instr->opd2 & IMMEDIATE_MASK, false);
    write_array(regs, assembly_instr->opd1, assembly_instr->opd2, false);
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
  case MOVE:
    write_array(regs, assembly_instr->opd2,
                read_array(regs, assembly_instr->opd1, false), false);
    if (assembly_instr->opd2 == PC) {
      goto no_pc_increase;
    }
    break;
  case NOP:
    break;
  case INT:
    setup_interrupt(assembly_instr->opd1);
    current_isr = assembly_instr->opd1;
    isr_active = true;
    goto no_pc_increase;
    break;
  case RTI:
    return_from_interrupt();
    if (!is_hardware_interrupt) {
      isr_active = false;
      step_into_activated = false;
    }
    if (heap_size > 0) {
      struct prio_isr ret_val = handle_next_hardware_interrupt();
      if (ret_val.has_higher_prio) {
        setup_interrupt(ret_val.isr);
      }
    }
    if (is_hardware_interrupt) {
      stack_top--;
      is_hardware_interrupt = false;
    }
    if (current_isr == isr_of_timer_interrupt) {
      interrupt_timer_active = true;
    }
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
    fprintf(stderr,
            "Error: A instruction with this opcode doesn't exist yet\n");
    exit(EXIT_FAILURE);
  }
  write_array(regs, PC, read_array(regs, PC, false) + 1, false);
no_pc_increase:;
}

void interpr_prgrm() {
  while (true) {
    if (debug_mode && breakpoint_encountered &&
        !(isr_active && !step_into_activated)) {
      update_term_and_box_sizes();
      draw_tui();
      evaluate_keyboard_input();
    }

    uint32_t machine_instr = read_storage(read_array(regs, PC, false));
    Instruction *assembly_instr = machine_to_assembly(machine_instr);

    if (assembly_instr->op == JUMP && assembly_instr->opd1 == 0) {
      free(assembly_instr);
      break;
    } else if (assembly_instr->op == INT && assembly_instr->opd1 == 3) {
      breakpoint_encountered = true;
      write_array(regs, PC, read_array(regs, PC, false) + 1, false);
    } else {
      interpr_instr(assembly_instr);
      free(assembly_instr);
    }

    timer_interrupt_check();
    uart_receive();
    uart_send();
  }
}

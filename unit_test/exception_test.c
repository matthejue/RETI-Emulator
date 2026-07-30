#include "../include/assemble.h"
#include "../include/exception.h"
#include "../include/interpr.h"
#include "../include/interrupt.h"
#include "../include/interrupt_controller.h"
#include "../include/parse/parse_args.h"
#include "../include/reti.h"
#include "../include/statemachine.h"
#include <assert.h>
#include <stdint.h>
#include <string.h>

#define TEST_EXCEPTION_HANDLER 100
#define TEST_FAULT_PC ((SRAM_CONST << 30) | 40)
#define TEST_STACK_POINTER ((SRAM_CONST << 30) | 1000)

static void prepare_exception(void) {
  memset(regs, 0, sizeof(uint32_t) * NUM_REGISTERS);
  init_cpu_exceptions();
  stacked_isrs_cnt = 0;
  hardware_isr_stack_top = -1;
  is_hardware_int_stack_top = -1;
  heap_size = 0;
  isr_num = CPU_EXCEPTION_ISR + 1;

  write_file(sram, CPU_EXCEPTION_ISR, TEST_EXCEPTION_HANDLER);
  write_array(regs, PC, TEST_FAULT_PC, false);
  write_array(regs, SP, TEST_STACK_POINTER, false);
}

static void assert_exception_frame(Cpu_Exception_Cause cause) {
  assert(cpu_exception_cause == cause);
  assert(read_storage((UART_CONST << 30) | CPU_EXCEPTION_CAUSE_REGISTER) ==
         cause);
  assert(read_array(regs, PC, false) ==
         ((SRAM_CONST << 30) | TEST_EXCEPTION_HANDLER));
  assert(read_array(regs, SP, false) == TEST_STACK_POINTER - 1);
  assert(read_storage(TEST_STACK_POINTER) == TEST_FAULT_PC - 1);
}

static void assert_divide_by_zero_exception(Instruction instr) {
  prepare_exception();
  write_array(regs, ACC, 123, false);
  write_array(regs, IN1, 0, false);
  write_array(regs, DS, SRAM_CONST << 30, false);
  write_storage((SRAM_CONST << 30) | 20, 0);

  interpr_instr(&instr);

  assert(read_array(regs, ACC, false) == 123);
  assert_exception_frame(CPU_EXCEPTION_DIVIDE_BY_ZERO);
}

static void test_divide_by_zero(void) {
  assert_divide_by_zero_exception(
      (Instruction){.op = DIVI, .opd1 = ACC, .opd2 = 0});
  assert_divide_by_zero_exception(
      (Instruction){.op = DIVR, .opd1 = ACC, .opd2 = IN1});
  assert_divide_by_zero_exception(
      (Instruction){.op = DIVM, .opd1 = ACC, .opd2 = 20});
  assert_divide_by_zero_exception(
      (Instruction){.op = MODI, .opd1 = ACC, .opd2 = 0});
  assert_divide_by_zero_exception(
      (Instruction){.op = MODR, .opd1 = ACC, .opd2 = IN1});
  assert_divide_by_zero_exception(
      (Instruction){.op = MODM, .opd1 = ACC, .opd2 = 20});
}

static void test_stack_overflow(void) {
  uint32_t boundary = TEST_STACK_POINTER - 1;
  prepare_exception();

  write_storage((UART_CONST << 30) | STACK_HEAP_BOUNDARY_REGISTER, boundary);
  assert(read_storage((UART_CONST << 30) | STACK_HEAP_BOUNDARY_REGISTER) ==
         boundary);

  interpr_instr(
      &(Instruction){.op = SUBI, .opd1 = SP, .opd2 = 2});

  // The rejected SUBI would produce boundary - 1
  // The exception entry consumes the one remaining interrupt-frame cell
  assert(read_array(regs, SP, false) == boundary);
  assert_exception_frame(CPU_EXCEPTION_STACK_OVERFLOW);
}

static void test_stack_boundary_is_inclusive(void) {
  uint32_t boundary = TEST_STACK_POINTER - 1;
  prepare_exception();
  write_storage((UART_CONST << 30) | STACK_HEAP_BOUNDARY_REGISTER, boundary);

  interpr_instr(
      &(Instruction){.op = SUBI, .opd1 = SP, .opd2 = 1});

  assert(read_array(regs, SP, false) == boundary);
  assert(read_array(regs, PC, false) == TEST_FAULT_PC + 1);
  assert(cpu_exception_cause == CPU_EXCEPTION_NONE);
}

static void test_stack_overflow_prevents_tsl_store(void) {
  uint32_t boundary = TEST_STACK_POINTER - 1;
  uint32_t tsl_address = (SRAM_CONST << 30) | 200;
  prepare_exception();
  write_storage((UART_CONST << 30) | STACK_HEAP_BOUNDARY_REGISTER, boundary);
  write_array(regs, IN1, tsl_address, false);
  write_storage(tsl_address, boundary - 1);

  interpr_instr(
      &(Instruction){.op = TSL, .opd1 = IN1, .opd2 = SP, .opd3 = 0});

  assert(read_storage(tsl_address) == boundary - 1);
  assert_exception_frame(CPU_EXCEPTION_STACK_OVERFLOW);
}

static void test_illegal_instruction(void) {
  prepare_exception();
  write_storage(TEST_FAULT_PC, (uint32_t)24 << 25);
  write_file(sram, TEST_EXCEPTION_HANDLER, (uint32_t)JUMP << 25);

  assert(!machine_word_is_valid_instruction(read_storage(TEST_FAULT_PC)));
  assert(!machine_word_is_valid_instruction((uint32_t)48 << 25));
  assert(!machine_word_is_valid_instruction((uint32_t)99 << 25));
  interpr_prgrm();

  assert_exception_frame(CPU_EXCEPTION_ILLEGAL_INSTRUCTION);
}

int main(void) {
  peripherals_dir = "/tmp";
  interrupt_timer_interval = 0;
  init_reti();

  test_divide_by_zero();
  test_stack_overflow();
  test_stack_boundary_is_inclusive();
  test_stack_overflow_prevents_tsl_store();
  test_illegal_instruction();

  fin_reti();
  return 0;
}

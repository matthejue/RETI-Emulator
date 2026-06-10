#include "../include/core_debug.h"
#include "../include/assemble.h"
#include "../include/parse_instrs.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include "assert.h"
#include <stdint.h>
#include "string.h"

void test_parse_instr() {
  const char *prgrm = "STOREIN    IN2   ACC   -2097152    ";
  String_Instruction *str_instr = parse_instr(&prgrm);
  assert(strcmp(str_instr->op, "STOREIN") == 0);
  assert(strcmp(str_instr->opd1, "IN2") == 0);
  assert(strcmp(str_instr->opd2, "ACC") == 0);
  assert(strcmp(str_instr->opd3, "-2097152") == 0);
}

void test_parse_instr2() {
  const char *prgrm = "ANDI ACC 42  ; \n";
  String_Instruction *str_instr = parse_instr(&prgrm);
  assert(strcmp(str_instr->op, "ANDI") == 0);
  assert(strcmp(str_instr->opd1, "ACC") == 0);
  assert(strcmp(str_instr->opd2, "42") == 0);
}

void test_parse_instr3() {
  const char *prgrm = "  RTI\n";
  String_Instruction *str_instr = parse_instr(&prgrm);
  assert(strcmp(str_instr->op, "RTI") == 0);
}

void test_parse_and_load_program() {
  peripherals_dir = "/tmp";
  init_reti();
  parse_and_load_program(
      allocate_and_copy_string("   LOADI   ACC 42  ;   \n    STOREIN IN2   ACC -2097152   ;\n  ADD ACC 32\n"), SRAM_PRGRM);
  char *str = assembly_to_str(machine_to_assembly(read_file(sram, 0)));
  assert(strcmp(str, "LOADI ACC 42") == 0);
  str = assembly_to_str(machine_to_assembly(read_file(sram, 1)));
  assert(strcmp(str, "STOREIN IN2 ACC -2097152") == 0);
  str = assembly_to_str(machine_to_assembly(read_file(sram, 2)));
  assert(strcmp(str, "ADD ACC 32") == 0);
  fin_reti();
}

void test_parse_and_load_program2() {
  peripherals_dir = "/tmp";
  init_reti();
  parse_and_load_program(
      allocate_and_copy_string("   JUMP<=  0;NOP   "), SRAM_PRGRM);
  char *str = assembly_to_str(machine_to_assembly(read_file(sram, 0)));
  assert(strcmp(str, "JUMP<= 0") == 0);
  str = assembly_to_str(machine_to_assembly(read_file(sram, 1)));
  assert(strcmp(str, "NOP") == 0);
  fin_reti();
}

void test_parse_and_load_numeric_memory_words() {
  peripherals_dir = "/tmp";
  init_reti();
  parse_and_load_program(
      allocate_and_copy_string("LOADI ACC 1; -1; 4294967295; 2147483648 # comment\nJUMP 0"),
      SRAM_PRGRM);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 0))),
                "LOADI ACC 1") == 0);
  assert(read_file(sram, 1) == (uint32_t)(int32_t)-1);
  assert(read_file(sram, 2) == UINT32_MAX);
  assert(read_file(sram, 3) == 2147483648U);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 4))),
                "JUMP 0") == 0);
  assert(num_instrs_prgrm == 5);
  fin_reti();
}

void test_parse_and_load_ascii_memory_words() {
  peripherals_dir = "/tmp";
  init_reti();
  parse_and_load_program(allocate_and_copy_string("'e'; '!'; JUMP 0"),
                         SRAM_PRGRM);

  assert(read_file(sram, 0) == 101);
  assert(read_file(sram, 1) == 33);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 2))),
                "JUMP 0") == 0);
  assert(num_instrs_prgrm == 3);
  fin_reti();
}

void test_unwritten_sram_words_read_as_zero() {
  peripherals_dir = "/tmp";
  init_reti();
  write_file(sram, 0, 111);

  assert(read_file(sram, 0) == 111);
  assert(read_file(sram, 1) == 0);

  fin_reti();
}

void test_parse_and_load_program_ranges() {
  peripherals_dir = "/tmp";
  ivt_max_idx = (uint32_t)-1;
  num_instrs_isrs = 0;
  num_instrs_prgrm = 0;
  isr_num = 0;
  init_reti();

  const char *program = "7; IVTE 3; LOADI ACC 7; ADDI ACC 1; 42; -1";
  parse_and_load_program_range(allocate_and_copy_string(program), ISR_PRGRMS, 0,
                               2);
  assert(num_instrs_isrs == 2);
  assert(ivt_max_idx == 1);
  assert(read_file(sram, 0) == 7);
  assert(read_file(sram, 1) == ((uint32_t)0b10 << 30 | 3));

  parse_and_load_program_range(allocate_and_copy_string(program), SRAM_PRGRM, 2,
                               4);
  assert(num_instrs_prgrm == 2);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 2))),
                "LOADI ACC 7") == 0);
  assert(strcmp(assembly_to_str(machine_to_assembly(read_file(sram, 3))),
                "ADDI ACC 1") == 0);

  parse_and_load_program_range(allocate_and_copy_string(program), SRAM_DATA, 4,
                               UINT32_MAX);
  assert(num_instrs_prgrm == 2);
  assert(read_file(sram, 4) == 42);
  assert(read_file(sram, 5) == (uint32_t)(int32_t)-1);

  fin_reti();
}

int main() {
  test_parse_instr();
  test_parse_instr2();
  test_parse_instr3();
  test_parse_and_load_program();
  test_parse_and_load_program2();
  test_parse_and_load_numeric_memory_words();
  test_parse_and_load_ascii_memory_words();
  test_unwritten_sram_words_read_as_zero();
  test_parse_and_load_program_ranges();
  return 0;
}

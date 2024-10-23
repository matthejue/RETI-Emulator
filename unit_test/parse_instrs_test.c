#include "../include/debug.h"
#include "../include/parse_instrs.h"
#include "../include/parse_args.h"
#include "../include/reti.h"
#include "../include/utils.h"
#include "assert.h"
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

int main() {
  test_parse_instr();
  test_parse_instr2();
  test_parse_instr3();
  test_parse_and_load_program();
  test_parse_and_load_program2();
  return 0;
}

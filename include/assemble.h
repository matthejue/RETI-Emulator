#include <stdbool.h>
#include <stdint.h>

#ifndef ASSEMBLE_H
#define ASSEMBLE_H

#define IMMEDIATE_MASK 0x3FFFFF // 22 bits for immediate value
#define TEN_BIT_MASK 0x3FF      // 22 bits for immediate value
/*#define ALL_BUT_IMMEDIATE_MASK 0xffc00000*/
#define REGISTER_MASK 0x7

extern uint8_t isr_of_timer_interrupt;
extern uint8_t isr_of_keypress_interrupt;
extern uint8_t isr_num;

#define INVALID_ISR_NUM UINT8_MAX

typedef enum {
  PC,
  IN1,
  IN2,
  ACC,
  SP,
  BAF,
  CS,
  DS,
  ADDRESS,
  CANCEL2 = 0b11111111
} Register;

typedef enum {
  ADDI,   // 0b0000000
  SUBI,   // 0b0000001
  MULTI,  // 0b0000010
  DIVI,   // 0b0000011
  MODI,   // 0b0000100
  OPLUSI, // 0b0000101
  ORI,    // 0b0000111
  ANDI,   // 0b0000111
  ADDR,   // 0b0001000
  SUBR,   // 0b0001001
  MULTR,  // 0b0001010
  DIVR,   // 0b0001011
  MODR,   // 0b0001100
  OPLUSR, // 0b0001101
  ORR,    // 0b0001110
  ANDR,   // 0b0001111
  ADDM,   // 0b0010000
  SUBM,   // 0b0010001
  MULTM,  // 0b0010010
  DIVM,   // 0b0010011
  MODM,   // 0b0010100
  OPLUSM, // 0b0010101
  ORM,    // 0b0010110
  ANDM,   // 0b0010111
  LOAD = 0b0100000,
  LOADIN = 0b0101000,
  LOADI = 0b0111000,
  STORE = 0b1000000,
  STOREIN = 0b1001000,
  MOVE = 0b1011000,
  NOP = 0b1100000,
  INT = 0b1100001,
  RTI = 0b1100010,
  JUMPGT = 0b1100100,
  JUMPEQ = 0b1101000,
  JUMPGE = 0b1101100,
  JUMPLT = 0b1110000,
  JUMPNE = 0b1110100,
  JUMPLE = 0b1111000,
  JUMP = 0b1111100,
} Unique_Opcode;

typedef enum { IVTE = 0b10000000, IVTEDP } Directive;

typedef enum { COMPUTE_M, LOAD_M, STORE_M, JUMP_M } mode;

extern const char *register_code_to_name[];

typedef struct {
  const char *name;
  Unique_Opcode value;
} String_to_Mnemonic;

typedef struct {
  const char *name;
  Directive value;
} String_to_Directive;

typedef struct {
  char op[8];
  char opd1[9];
  char opd2[9];
  char opd3[9]; // because len("-2097152") = 8 + null-terminator
} String_Instruction;

typedef struct {
  uint8_t op;
  uint32_t opd1;
  uint32_t opd2;
  uint32_t opd3;
} Instruction;

uint8_t get_register_code(char *reg);
uint8_t get_mnemonic(char *mnemonic);
Instruction *machine_to_assembly(uint32_t machine_instr);
uint32_t assembly_to_machine(String_Instruction *str_instr);

#endif // ASSEMBLE_H

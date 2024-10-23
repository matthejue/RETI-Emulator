#include "../include/assemble.h"
#include <stdint.h>

#ifndef ERROR_H
#define ERROR_H

struct ErrorContext {
  const char *filename;
  const char *code_current;
  const char *code_begin;
};

extern struct ErrorContext error_context;

typedef enum {Pntr, Idx} ErrorContextType;

typedef enum {
  REG,
  IM,
  EMPTY,
} OperandType;

void display_error_message(const char *error_type, const char *error_message, const char *to_insert, ErrorContextType error_context_type) ;
void check_instr(uint8_t op, String_Instruction *str_instr);

#endif // ERROR_H

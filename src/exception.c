#include "../include/exception.h"
#include "../include/assemble.h"
#include "../include/parse/parse_args.h"
#include "../include/special_opts.h"
#include "../include/statemachine.h"
#include <stdlib.h>

uint32_t stack_heap_boundary = 0;
Cpu_Exception_Cause cpu_exception_cause = CPU_EXCEPTION_NONE;

static bool stack_protection_active = false;

void init_cpu_exceptions(void) {
  stack_heap_boundary = 0;
  stack_protection_active = false;
  cpu_exception_cause = CPU_EXCEPTION_NONE;
}

bool stack_pointer_would_overflow(uint32_t stack_pointer) {
  return stack_protection_active && stack_pointer < stack_heap_boundary;
}

static const char *exception_name(Cpu_Exception_Cause cause) {
  switch (cause) {
  case CPU_EXCEPTION_DIVIDE_BY_ZERO:
    return "DivisionByZeroError";
  case CPU_EXCEPTION_STACK_OVERFLOW:
    return "StackOverflowError";
  case CPU_EXCEPTION_ILLEGAL_INSTRUCTION:
    return "IllegalInstructionError";
  default:
    return "UnknownCpuException";
  }
}

void trigger_cpu_exception(Cpu_Exception_Cause cause) {
  cpu_exception_cause = cause;

  if (isr_num <= CPU_EXCEPTION_ISR) {
    const char *name = exception_name(cause);
    adjust_print(false, "Unhandled CPU exception: %s\n", NULL, name);
    adjust_print(true, NULL, "%s", name);
    exit(test_mode ? EXIT_SUCCESS : EXIT_FAILURE);
  }

  in.arg8 = CPU_EXCEPTION_ISR;
  update_state(CPU_EXCEPTION);
}

void set_stack_heap_boundary(uint32_t boundary) {
  stack_heap_boundary = boundary;
  stack_protection_active = boundary != 0;
}

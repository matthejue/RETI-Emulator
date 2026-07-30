#ifndef EXCEPTION_H
#define EXCEPTION_H

#include <stdbool.h>
#include <stdint.h>

#define CPU_EXCEPTION_ISR 3

typedef enum {
  CPU_EXCEPTION_NONE = 0,
  CPU_EXCEPTION_DIVIDE_BY_ZERO,
  CPU_EXCEPTION_STACK_OVERFLOW,
  CPU_EXCEPTION_ILLEGAL_INSTRUCTION
} Cpu_Exception_Cause;

extern uint32_t stack_heap_boundary;
extern Cpu_Exception_Cause cpu_exception_cause;

void init_cpu_exceptions(void);
bool stack_pointer_would_overflow(uint32_t stack_pointer);
void set_stack_heap_boundary(uint32_t boundary);
void trigger_cpu_exception(Cpu_Exception_Cause cause);

#endif // EXCEPTION_H

#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRUPT_H
#define INTERRUPT_H

extern uint32_t interrupt_timer_interval;

extern bool interrupt_timer_active;

extern bool keypress_interrupt_active;
extern bool keypress_interrupt_activatable;

extern uint32_t timer_cnt;

void do_step_into_isr();

bool timer_interrupt_check();
bool keypress_interrupt_trigger();

#endif // INTERRUPT_H

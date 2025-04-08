#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRUPT_H
#define INTERRUPT_H

extern uint32_t interrupt_timer_interval;
extern bool interrupt_timer_active;

extern bool keypress_interrupt_active;

void timer_interrupt_check();
void keypress_interrupt_trigger();

#endif // INTERRUPT_H

#include <stdbool.h>
#include <stdint.h>

#ifndef INTERRUPT_H
#define INTERRUPT_H

extern uint32_t interrupt_timer_interval;

extern bool interrupt_timer_active;

extern bool custom_interrupt_active;
extern bool custom_interrupt_activatable;

extern uint32_t timer_cnt;

void do_step_into_isr();

bool timer_interrupt_check();
bool custom_interrupt_trigger();
bool uart_interrupt_trigger(uint8_t byte);
void uart_interrupt_completed(uint8_t isr);
void init_custom_interrupt_action_isr(void);
bool cycle_custom_interrupt_action_isr(void);
uint8_t get_custom_interrupt_action_isr(void);

#endif // INTERRUPT_H

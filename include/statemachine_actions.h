#include <stdbool.h>

#ifndef STATEMACHINE_ACTIONS_H
#define STATEMACHINE_ACTIONS_H

void finish_if_finished_here(void);
void step_out_if_stepped_into_here(void);
bool check_prio_hi(void);
bool check_prio_heap(void);
void insert_into_heap(void);
void handle_next_hi(void);
void no_si_inside_hi_error(void);
bool check_if_si_happened_here(void);

#endif // STATEMACHINE_ACTIONS_H

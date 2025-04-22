#ifndef STATEMACHINE_H
#define STATEMACHINE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    HI,
    NB,
    SI,
} State;

typedef enum {
    FINALIZE,
    INT,
    INT_I,
    RTI,
    STEP_INTO_ACTION,
} Event;

extern State current_state;
extern bool breakpoint_encountered;
extern bool isr_finished;
extern bool step_into_activated;
extern bool isr_active;
extern bool si_happened;
extern uint8_t finished_here;
extern uint8_t stepped_into_here;

void update_state(Event event);

#endif // STATEMACHINE_H

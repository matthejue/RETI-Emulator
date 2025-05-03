#include "../include/statemachine.h"

#ifndef LOG_VARIABLE_H
#define LOG_VARIABLE_H

extern bool debug_activated;

void log_variable(const char *file_name, const char *var_name, int value);
void log_statemachine(Event event);
void debug();

#endif // LOG_VARIABLE_H

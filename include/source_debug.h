#ifndef SOURCE_DEBUG_H
#define SOURCE_DEBUG_H

#include <stdbool.h>
#include <stdint.h>

extern const char *current_stackframe_function;

bool start_source_debugger(void);
void write_source_debug_state(void);
void stop_source_debugger(void);
void source_debug_update_current_stackframe_function(void);
const char *source_debug_variable_label_for_sram_idx(uint64_t idx);

#endif // SOURCE_DEBUG_H

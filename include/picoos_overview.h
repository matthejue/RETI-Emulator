#ifndef PICOOS_OVERVIEW_H
#define PICOOS_OVERVIEW_H

#include "parse/parse_sections.h"
#include <stdbool.h>
#include <stdint.h>

void set_picoos_overview_sections(Program_Sections sections);
void reset_picoos_overview(bool synthetic_startup);
bool picoos_overview_is_available(void);
bool start_picoos_overview(void);
bool picoos_overview_is_open(void);
bool picoos_overview_take_navigation_request(uint32_t *start, uint32_t *end);
void refresh_picoos_overview(void);
void stop_picoos_overview(void);

void picoos_overview_log_software_interrupt(uint8_t isr,
                                            uint32_t syscall_number,
                                            uint32_t argument);
void picoos_overview_log_hardware_interrupt(const char *source, uint8_t isr,
                                            uint32_t detail);
void picoos_overview_enter_hardware_interrupt(uint8_t isr);
void picoos_overview_log_cpu_exception(uint8_t isr, uint32_t cause);
void picoos_overview_interrupt_returned(void);
void picoos_overview_log_host_request(const char *request);

#endif // PICOOS_OVERVIEW_H

#include <stdbool.h>
#include <stdint.h>

#ifndef PARSE_SECTIONS_H
#define PARSE_SECTIONS_H

#define STACK_START_AUTO UINT32_MAX

typedef struct {
  bool exists;
  uint32_t codesegment_start;
  uint32_t datasegment_start;
  uint32_t stack_start;
  bool has_stack_start;
} Program_Sections;

Program_Sections parse_sections_for_reti_path(const char *reti_path);
Program_Sections parse_required_section_for_reti_path(const char *reti_path);
char *sections_path_for_reti_path(const char *reti_path);

#endif // PARSE_SECTIONS_H

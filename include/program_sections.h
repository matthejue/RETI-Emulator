#include <stdbool.h>
#include <stdint.h>

#ifndef PROGRAM_SECTIONS_H
#define PROGRAM_SECTIONS_H

typedef struct {
  bool exists;
  uint32_t codesegment_start;
  uint32_t datasegment_start;
} Program_Sections;

Program_Sections load_program_sections_for_reti_path(const char *reti_path);
char *program_sections_path_for_reti_path(const char *reti_path);

#endif // PROGRAM_SECTIONS_H

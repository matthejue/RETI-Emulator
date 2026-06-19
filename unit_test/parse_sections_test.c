#include "../include/parse/parse_sections.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void write_test_file(const char *path, const char *content) {
  FILE *file = fopen(path, "w");
  assert(file != NULL);
  fputs(content, file);
  fclose(file);
}

void test_sections_path_for_reti_path(void) {
  char *path = sections_path_for_reti_path("/tmp/example.reti");
  assert(strcmp(path, "/tmp/example.sections") == 0);
  free(path);

  path = sections_path_for_reti_path("/tmp/example");
  assert(strcmp(path, "/tmp/example.sections") == 0);
  free(path);

  path = sections_path_for_reti_path("-");
  assert(path == NULL);
}

void test_parse_sections_for_reti_path(void) {
  const char *reti_path = "/tmp/reti_sections_test.reti";
  const char *sections_path = "/tmp/reti_sections_test.sections";
  write_test_file(sections_path,
                  "{ \"codesegment_start\": 40, \"datasegment_start\": 180 }");

  Program_Sections sections = parse_sections_for_reti_path(reti_path);
  assert(sections.exists);
  assert(sections.codesegment_start == 40);
  assert(sections.datasegment_start == 180);
  assert(!sections.has_stack_start);

  remove(sections_path);
}

void test_section_path_for_reti_path(void) {
  char *path = section_path_for_reti_path("/tmp/example.reti");
  assert(strcmp(path, "/tmp/example.section") == 0);
  free(path);

  path = section_path_for_reti_path("/tmp/example");
  assert(strcmp(path, "/tmp/example.section") == 0);
  free(path);
}

void test_parse_required_section_for_reti_path(void) {
  const char *reti_path = "/tmp/reti_required_section_test.reti";
  const char *section_path = "/tmp/reti_required_section_test.section";
  write_test_file(section_path,
                  "{ \"codesegment_start\": 0, \"datasegment_start\": 4572, "
                  "\"stack_start\": 8000 }");

  Program_Sections sections = parse_required_section_for_reti_path(reti_path);
  assert(sections.exists);
  assert(sections.codesegment_start == 0);
  assert(sections.datasegment_start == 4572);
  assert(sections.stack_start == 8000);
  assert(sections.has_stack_start);

  remove(section_path);
}

void test_missing_parse_sections_file(void) {
  Program_Sections sections =
      parse_sections_for_reti_path("/tmp/reti_sections_missing.reti");
  assert(!sections.exists);
}

int main(void) {
  test_sections_path_for_reti_path();
  test_section_path_for_reti_path();
  test_parse_sections_for_reti_path();
  test_parse_required_section_for_reti_path();
  test_missing_parse_sections_file();
  return 0;
}

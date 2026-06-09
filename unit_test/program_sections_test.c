#include "../include/program_sections.h"
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

void test_program_sections_path_for_reti_path(void) {
  char *path = program_sections_path_for_reti_path("/tmp/example.reti");
  assert(strcmp(path, "/tmp/example.sections") == 0);
  free(path);

  path = program_sections_path_for_reti_path("/tmp/example");
  assert(strcmp(path, "/tmp/example.sections") == 0);
  free(path);

  path = program_sections_path_for_reti_path("-");
  assert(path == NULL);
}

void test_load_program_sections_for_reti_path(void) {
  const char *reti_path = "/tmp/reti_sections_test.reti";
  const char *sections_path = "/tmp/reti_sections_test.sections";
  write_test_file(sections_path,
                  "{ \"codesegment_start\": 40, \"datasegment_start\": 180 }");

  Program_Sections sections = load_program_sections_for_reti_path(reti_path);
  assert(sections.exists);
  assert(sections.codesegment_start == 40);
  assert(sections.datasegment_start == 180);

  remove(sections_path);
}

void test_missing_program_sections_file(void) {
  Program_Sections sections =
      load_program_sections_for_reti_path("/tmp/reti_sections_missing.reti");
  assert(!sections.exists);
}

int main(void) {
  test_program_sections_path_for_reti_path();
  test_load_program_sections_for_reti_path();
  test_missing_program_sections_file();
  return 0;
}

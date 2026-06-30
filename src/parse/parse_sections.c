#include "../../include/parse/parse_sections.h"
#include "../../include/parse/parse_args.h"
#include "../../include/utils.h"
#include "../../vendor/cJSON/cJSON.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool file_exists(const char *path) {
  FILE *file = fopen(path, "r");
  if (file == NULL) {
    return false;
  }
  fclose(file);
  return true;
}

static char *path_for_reti_path_with_extension(const char *reti_path,
                                               const char *extension) {
  if (strcmp(reti_path, "-") == 0) {
    return NULL;
  }

  size_t path_len = strlen(reti_path);
  const char *reti_suffix = ".reti";
  size_t suffix_len = strlen(reti_suffix);

  if (path_len >= suffix_len &&
      strcmp(reti_path + path_len - suffix_len, reti_suffix) == 0) {
    size_t basename_len = path_len - suffix_len;
    char *sections_path = malloc(basename_len + strlen(extension) + 1);
    if (sections_path == NULL) {
      fprintf(stderr, "Failed to allocate memory\n");
      exit(EXIT_FAILURE);
    }
    strncpy(sections_path, reti_path, basename_len);
    sections_path[basename_len] = '\0';
    strcat(sections_path, extension);
    return sections_path;
  }

  return proper_str_cat(reti_path, extension);
}

char *sections_path_for_reti_path(const char *reti_path) {
  return path_for_reti_path_with_extension(reti_path, ".sections");
}

static uint32_t read_uint32_section_value(cJSON *root, const char *key,
                                          const char *path, bool required,
                                          bool *exists) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, key);
  if (item == NULL) {
    if (required) {
      fprintf(stderr, "Error: Missing \"%s\" in %s\n", key, path);
      exit(EXIT_FAILURE);
    }
    *exists = false;
    return 0;
  }

  if (!cJSON_IsNumber(item) || item->valuedouble < 0 ||
      item->valuedouble > UINT32_MAX ||
      item->valuedouble != (uint32_t)item->valuedouble) {
    fprintf(stderr, "Error: \"%s\" in %s must be an unsigned 32-bit integer\n",
            key, path);
    exit(EXIT_FAILURE);
  }

  *exists = true;
  return (uint32_t)item->valuedouble;
}

static uint32_t read_stack_start_section_value(cJSON *root, const char *path,
                                               bool required, bool *exists) {
  cJSON *item = cJSON_GetObjectItemCaseSensitive(root, "stack_start");
  if (item == NULL) {
    if (required) {
      fprintf(stderr, "Error: Missing \"stack_start\" in %s\n", path);
      exit(EXIT_FAILURE);
    }
    *exists = false;
    return STACK_START_AUTO;
  }

  if (!cJSON_IsNumber(item) || item->valuedouble < -1 ||
      item->valuedouble > UINT32_MAX ||
      item->valuedouble != (int64_t)item->valuedouble) {
    fprintf(stderr,
            "Error: \"stack_start\" in %s must be -1 or an unsigned 32-bit integer\n",
            path);
    exit(EXIT_FAILURE);
  }

  *exists = true;
  return item->valuedouble == -1 ? STACK_START_AUTO : (uint32_t)item->valuedouble;
}

static Program_Sections parse_sections_file(const char *sections_path,
                                            bool require_stack_start) {
  Program_Sections sections = {.exists = false,
                               .codesegment_start = 0,
                               .datasegment_start = 0,
                               .heap_start = 0,
                               .interrupt_service_routines_start = 0,
                               .stack_start = STACK_START_AUTO,
                               .has_heap_start = false,
                               .has_stack_start = false,
                               .has_interrupt_service_routines_start = false};
  char *content = read_file_content(sections_path);
  cJSON *root = cJSON_Parse(content);
  if (root == NULL) {
    fprintf(stderr, "Error: Failed to parse %s as JSON\n", sections_path);
    free(content);
    exit(EXIT_FAILURE);
  }

  bool exists;
  sections.exists = true;
  sections.codesegment_start = read_uint32_section_value(
      root, "codesegment_start", sections_path, true, &exists);
  sections.datasegment_start = read_uint32_section_value(
      root, "datasegment_start", sections_path, true, &exists);
  sections.heap_start = read_uint32_section_value(
      root, "heap_start", sections_path, require_stack_start,
      &sections.has_heap_start);
  sections.interrupt_service_routines_start = read_uint32_section_value(
      root, "interrupt_service_routines_start", sections_path, false,
      &sections.has_interrupt_service_routines_start);
  sections.stack_start = read_stack_start_section_value(
      root, sections_path, require_stack_start, &sections.has_stack_start);

  cJSON_Delete(root);
  free(content);
  return sections;
}

Program_Sections parse_sections_for_reti_path(const char *reti_path) {
  Program_Sections sections = {.exists = false,
                               .codesegment_start = 0,
                               .datasegment_start = 0,
                               .heap_start = 0,
                               .interrupt_service_routines_start = 0,
                               .stack_start = STACK_START_AUTO,
                               .has_heap_start = false,
                               .has_stack_start = false,
                               .has_interrupt_service_routines_start = false};
  bool explicit_sections_path = strcmp(sections_path, "") != 0;
  char *default_sections_path = NULL;
  const char *path =
      explicit_sections_path ? sections_path
                             : (default_sections_path =
                                    sections_path_for_reti_path(reti_path));
  if (path == NULL) {
    return sections;
  }
  if (!file_exists(path)) {
    free(default_sections_path);
    return sections;
  }

  sections = parse_sections_file(path, false);
  free(default_sections_path);
  return sections;
}

Program_Sections parse_required_section_for_reti_path(const char *reti_path) {
  bool explicit_sections_path = strcmp(sections_path, "") != 0;
  char *default_sections_path = NULL;
  const char *path =
      explicit_sections_path ? sections_path
                             : (default_sections_path =
                                    sections_path_for_reti_path(reti_path));
  if (path == NULL || !file_exists(path)) {
    fprintf(stderr, "Error: Assemble mode requires section file %s\n",
            path == NULL ? "<stdin>.sections" : path);
    free(default_sections_path);
    exit(EXIT_FAILURE);
  }

  Program_Sections sections = parse_sections_file(path, true);
  free(default_sections_path);
  return sections;
}

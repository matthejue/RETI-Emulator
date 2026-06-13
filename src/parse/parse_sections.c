#include "../../include/parse/parse_sections.h"
#include "../../include/utils.h"
#include "../../vendor/cJSON/cJSON.h"
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

char *sections_path_for_reti_path(const char *reti_path) {
  if (strcmp(reti_path, "-") == 0) {
    return NULL;
  }

  size_t path_len = strlen(reti_path);
  const char *reti_suffix = ".reti";
  size_t suffix_len = strlen(reti_suffix);

  if (path_len >= suffix_len &&
      strcmp(reti_path + path_len - suffix_len, reti_suffix) == 0) {
    size_t basename_len = path_len - suffix_len;
    char *sections_path = malloc(basename_len + strlen(".sections") + 1);
    if (sections_path == NULL) {
      fprintf(stderr, "Failed to allocate memory\n");
      exit(EXIT_FAILURE);
    }
    strncpy(sections_path, reti_path, basename_len);
    sections_path[basename_len] = '\0';
    strcat(sections_path, ".sections");
    return sections_path;
  }

  return proper_str_cat(reti_path, ".sections");
}

Program_Sections parse_sections_for_reti_path(const char *reti_path) {
  Program_Sections sections = {.exists = false,
                               .codesegment_start = 0,
                               .datasegment_start = 0};
  char *sections_path = sections_path_for_reti_path(reti_path);
  if (sections_path == NULL) {
    return sections;
  }
  if (!file_exists(sections_path)) {
    free(sections_path);
    return sections;
  }

  char *content = read_file_content(sections_path);
  cJSON *root = cJSON_Parse(content);
  if (root == NULL) {
    fprintf(stderr, "Error: Failed to parse %s as JSON\n", sections_path);
    free(content);
    free(sections_path);
    exit(EXIT_FAILURE);
  }

  sections.exists = true;
  sections.codesegment_start =
      cJSON_GetObjectItemCaseSensitive(root, "codesegment_start")->valueint;
  sections.datasegment_start =
      cJSON_GetObjectItemCaseSensitive(root, "datasegment_start")->valueint;

  cJSON_Delete(root);
  free(content);
  free(sections_path);
  return sections;
}

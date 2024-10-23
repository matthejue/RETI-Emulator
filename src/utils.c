#include "../include/utils.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// r = a % m <-> r + m * q = a + m * p <-> r = a - m * q (q is biggest q such
// that q*m <= a) = a − m * (a / m) 0 <= r < m, a,q € Z, m € N/0 (normal C /
// hardware implementation ignores 0 <= r < m) 7 % 3 = 7 - 3 * (7 / 3) = 1 -7 %
// 3 = -7 - 3 * (-7 / 3) = -1, but r has to be positive, get next congruent
// element of congruence class by -1 + 3 = 2 (one can easily get negative or
// positive r with -m < r < m by just nr = r - m and also r = nr + m) irrelevant
// cases where on ignores m € N/0: 7 % -3 = 7 - (-3) * (7 / -3) = 1 (if we
// ignore m € N/0), but that is only for truncated division, for floor it is 7 -
// (-3) * (7 / -3) = 7 - (-3) * -3 = 7 - 9 = -2 (if we ignore 0 <= r < m and say
// -m < r < m) -7 % -3 = -7 - (-3) * (-7 / -3) = -1 (if we ignore m € N/0 and 0
// =< r < m and thus -m < r < m), for floor it is the same: -7 - (-3) * (-7 /
// -3) = -7 - (-3) * 2 = -7 - (-6) = -1, -1 + 3 = 2 (if don't want to also
// ignore 0 =< r < m)

uint32_t mod(int32_t a, int32_t b) {
  // r = a − m x trunc(a/m)
  int32_t result = a % b;
  if (result < 0) {
    result += b;
  }
  return result;
}

int64_t max(int64_t a, int64_t b) { return (a > b) ? a : b; }

int64_t min(int64_t a, int64_t b) { return (a < b) ? a : b; }

uint32_t sign_extend_22_to_32(uint32_t num) {
  if (num & (1 << 21)) {
    num |= ~((1 << 22) - 1);
  } else {
    num &= (1 << 22) - 1;
  }
  return num;
}

uint32_t swap_endian_32(uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0x0000FF00) |
         ((value << 8) & 0x00FF0000) | (value << 24);
}

char *proper_str_cat(const char *prefix, const char *suffix) {
  char *new_file_dir = malloc(strlen(prefix) + strlen(suffix) + 1);
  strcpy(new_file_dir, prefix);
  return strcat(new_file_dir, suffix);
}

char *read_stdin_content() {
  size_t buffer_size = INITIAL_BUFFER_SIZE;
  size_t content_size = 0;
  char *content = malloc(buffer_size);
  if (content == NULL) {
    fprintf(stderr, "Failed to allocate memory\n");
    exit(1);
  }

  size_t bytes_read;
  while ((bytes_read = fread(content + content_size, 1,
                             buffer_size - content_size, stdin)) > 0) {
    content_size += bytes_read;
    if (content_size == buffer_size) {
      buffer_size *= 2;
      content = realloc(content, buffer_size);
      if (content == NULL) {
        fprintf(stderr, "Failed to reallocate memory\n");
        exit(1);
      }
    }
  }

  // Null-terminate the content
  content[content_size] = '\0';

  return content;
}

char *read_file_content(const char *file_path) {
  FILE *file = fopen(file_path, "r");
  if (!file) {
    fprintf(stderr, "Error opening file");
    exit(EXIT_FAILURE);
  }

  fseek(file, 0, SEEK_END);
  long file_size = ftell(file);
  fseek(file, 0, SEEK_SET);

  char *content = malloc(file_size + 1);
  if (!content) {
    fprintf(stderr, "Error allocating memory");
    exit(EXIT_FAILURE);
  }

  fread(content, 1, file_size, file);
  content[file_size] = '\0';

  fclose(file);
  return content;
}

char *get_prgrm_content(const char *prgrm_path) {
  if (strcmp(prgrm_path, "-") == 0) {
    return read_stdin_content();
  } else {
    FILE *file = fopen(prgrm_path, "r");
    if (file) {
      fclose(file);
      return read_file_content(prgrm_path);
    } else {
      fprintf(stderr, "Error: Unable to open file %s\n", prgrm_path);
      exit(EXIT_FAILURE);
    }
  }
}

char *allocate_and_copy_string(const char *original) {
  size_t length = strlen(original) + 1; // +1 for the null terminator

  // Allocate memory
  char *copy = (char *)malloc(length);
  if (copy == NULL) {
    fprintf(stderr, "Memory allocation failed\n");
    return NULL;
  }

  // Copy the string
  strcpy(copy, original);

  return copy;
}

char *extract_line(const char *current, const char *begin) {
  // Find the start of the line
  const char *start = current;
  while (start >= begin && *(start - 1) != '\n') {
    start--;
  }

  // Find the end of the line
  const char *end = current;
  while (*end != '\0' && *end != '\n') {
    end++;
  }

  // Calculate the length of the line
  size_t length = end - start;

  // Allocate memory for the result and copy the line
  char *result = (char *)malloc(length + 1);
  if (result == NULL) {
    return NULL; // Memory allocation failed
  }
  strncpy(result, start, length);
  result[length] = '\0';

  return result;
}

int count_lines(const char *current, const char *begin) {
  int line_count = 1;
  const char *ptr = begin;

  while (ptr < current) {
    if (*ptr == '\n') {
      line_count++;
    }
    ptr++;
  }

  return line_count;
}

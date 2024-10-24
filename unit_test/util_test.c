#include "../include/utils.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_extract_line() {
  const char *text = "Line 1\nLine 2\nLine 3\nLine 4\nLine 5\nLine 6\nLine 7\n";
  const char *ptr = text + 15; // Pointer to 'i' in "Line 3"

  char *result = extract_line(ptr, text);
  assert(strcmp(result, "Line 3") == 0);
  free(result);
}

void test_count_lines() {
  const char *text = "Line 1\nLine 2\nLine 3\nLine 4\n";
  const char *current = text + 15; // Arbitrary position in the string

  int lines = count_lines(current, text);
  assert(lines == 3);
}


void test_int_to_bin_str() {
  assert(strcmp(int_to_bin_str(6, 3), "110") == 0);
}

int main() {
  test_extract_line();
  test_count_lines();
  test_int_to_bin_str();

  return 0;
}

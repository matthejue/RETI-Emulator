#include "../include/special_opts.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_extract_comment_metadata() {
  char *filename = "/tmp/extract_comment_metadata.reti";
  FILE *file = fopen(filename, "w");
  if (file == NULL) {
    fprintf(stderr, "Error: File could not be opened\n");
    exit(EXIT_FAILURE);
  }
  fprintf(file, "# input: 72 ello 32 Wo 114 ld\n");
  fprintf(file, "JUMP 0\n");
  fclose(file);
  uint8_t len = 0;
  uint32_t *comment_metadata = extract_comment_metadata(filename, &len);
  uint32_t expected[] = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd'};
  for (uint8_t i = 0; i < len; i++) {
    assert(comment_metadata[i] == expected[i]);
  }
}

int main() {
  test_extract_comment_metadata();
  return 0;
}

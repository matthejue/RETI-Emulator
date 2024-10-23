#include <stdint.h>

#ifndef UTILS_H
#define UTILS_H

#define INITIAL_BUFFER_SIZE 1024

uint32_t mod(int32_t a, int32_t b);
int64_t max(int64_t a, int64_t b);
int64_t min(int64_t a, int64_t b);
int32_t sign_extend_10_to_32(uint32_t num);
uint32_t sign_extend_22_to_32(uint32_t num);
uint32_t swap_endian_32(uint32_t value);
char *proper_str_cat(const char *prefix, const char *suffix);
char *read_stdin_content();
char *read_file_content(const char *file_path);
char *get_prgrm_content(const char *prgrm_path);
char *allocate_and_copy_string(const char *original);
char *extract_line(const char *current, const char *begin) ;
int count_lines(const char *current, const char *begin) ;
char *create_heading(char insert_chr, const char* text, int linewidth) ;

#endif // UTILS_H

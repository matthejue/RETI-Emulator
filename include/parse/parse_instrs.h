#include "../assemble.h"
#include <stdbool.h>

#ifndef PARSE_PARSE_INSTRS_H
#define PARSE_PARSE_INSTRS_H

typedef enum {
  EPROM_START_PRGRM,
  SRAM_PRGRM,
  SRAM_DATA,
  ISR_PRGRMS
} Program_Type;

typedef enum { COMMENT_TARGET_EPROM, COMMENT_TARGET_SRAM } Comment_Target;

typedef struct {
  Comment_Target target;
  uint32_t anchor_idx;
  bool display_before_instr;
  char *text;
} Source_Comment;

extern Source_Comment *source_comments;
extern uint32_t num_source_comments;

String_Instruction *parse_instr(const char **orignal_prgrm_pntr);
void collect_program_comments(const char *prgrm, Program_Type memory_type);
void collect_program_comments_range(const char *prgrm, Program_Type prgrm_type,
                                    uint32_t start_entry, uint32_t end_entry,
                                    uint32_t anchor_base);
void parse_and_load_program(char *prgrm, Program_Type memory_type) ;
void parse_and_load_program_range(char *prgrm, Program_Type memory_type,
                                  uint32_t start_entry, uint32_t end_entry);
void free_program_comments(void);

#endif // PARSE_PARSE_INSTRS_H

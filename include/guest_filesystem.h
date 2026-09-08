#ifndef GUEST_FILESYSTEM_H
#define GUEST_FILESYSTEM_H

#include <stdbool.h>
#include <stdio.h>

bool init_guest_filesystem(void);
void close_guest_filesystem(void);
FILE *guest_fopen(const char *path, const char *mode);
bool guest_is_directory(const char *path);
char *guest_list_directory(const char *path);
int guest_mkdir(const char *path);
int guest_unlink(const char *path);
int guest_rmdir(const char *path);
int guest_move(const char *old_path, const char *new_path);
int guest_touch(const char *path);

#endif

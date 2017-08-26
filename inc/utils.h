#pragma once 

#include "../inc/commons.h"

char *my_basename(char *buff, int size, const char *str);
void update_directorysizes(const char *name, const char *fullp, int index);
int update_info(const char *oldpath, const char *newname, int index);
void remove_line_from_directorysizes(const char *path, int index);
int get_correct_topdir_idx(const char *path);
int get_idx_from_wd(const int wd);

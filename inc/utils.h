#pragma once 

#include "../inc/commons.h"

char *my_basename(char *buff, int size, const char *str);
void update_directorysizes(const char *name, const char *fullp);
int update_info(const char *oldpath, const char *newname);
void remove_line_from_directorysizes(const char *filename);

#pragma once 

#include "../inc/commons.h"

char *my_basename(char *buff, int size, const char *str);
int init_trash(const char *root, const char *dev_path);
int init_local_trash(void);
void update_directorysizes(const char *name, const char *fullp, int index);
int update_info(const char *oldpath, const char *newname, int index);
void remove_line_from_directorysizes(const char *path, int index);
int get_correct_topdir_idx(const char *path);
int get_idx_from_devpath(const char *devpath);
char *get_mountpoint(const char *dev_path);
int get_idx_from_wd(const int wd);
void destroy_trash(void);
void remove_trash(int index);

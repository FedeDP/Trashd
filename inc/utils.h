#pragma once 

#include "../inc/commons.h"

char *my_basename(char *buff, int size, const char *str);
int init_trash(const char *root, const char *devpath, int external);
int init_local_trash(void);
void update_directorysizes(const char *name, const char *fullp, int index);
int update_info(const char *oldpath, const char *newname, int index);
void remove_line_from_directorysizes(const char *path, int index);
int is_dir(const char *path);
int my_rename(const char *orig, char *newname, const size_t size);
int get_correct_topdir_idx(const char *path);
int get_idx_from_devpath(const char *devpath);
char *get_mountpoint(const char *dev_path);
void remove_trash(int index);

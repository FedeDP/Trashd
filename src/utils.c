#include "../inc/utils.h"
#include <libgen.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

static int sum_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static long unsigned int compute_size(const char *path);

char *my_basename(char *buff, int size, const char *str) {
    strncpy(buff, str, size);
    return basename(buff);
}

void update_directorysizes(const char *name, const char *fullp) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash_path);
    FILE *f = fopen(p, "a");
    if (f) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        fprintf(f, "%lu %lu %s\n", compute_size(fullp), tv.tv_sec, name);
        fclose(f);
    }
}

int update_info(const char *oldpath, const char *newname) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/%s.trashinfo", info_path, newname);
    FILE *f = fopen(p, "w");
    if (f) {
        time_t rawtime;
        struct tm *info;
        
        time(&rawtime);
        info = localtime(&rawtime);
        
        fprintf(f, "[Trash Info]\n");
        fprintf(f, "Path=%s\n", oldpath);
        fprintf(f, "DeletionDate=%d-%d-%dT%d:%d:%d\n", info->tm_year + 1900, info->tm_mon + 1, info->tm_mday,
                info->tm_hour, info->tm_min, info->tm_sec);
        fclose(f);
        return 0;
    }
    return -1;
}

void remove_line_from_directorysizes(const char *filename) {
    int ok = 0;
    
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash_path);
    FILE *f = fopen(p, "r");
    
    if (f) {
        char ptmp[PATH_MAX + 1] = {0};
        snprintf(ptmp, PATH_MAX, "%s/directorysizes.tmp", trash_path);
        FILE *ftmp = fopen(ptmp, "w");
        
        if (ftmp) {
            ok = 1;
            char name[NAME_MAX + 1] = {0};
            unsigned long int t, size;
            while (!feof(f)) {
                fscanf(f, "%lu %lu %s\n", &size, &t, name);
                if (strcmp(name, filename)) {
                    fprintf(ftmp, "%lu %lu %s\n", size, t, name);
                }
            }
            fclose(ftmp);
        }
        fclose(f);
        if (ok) {
            /* Remove old directorysizes file */
            remove(p);
            /* rename new directorysizes file */
            rename(ptmp, p);
        }
    }
}

static int sum_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    total_size += sb->st_blocks * 512;
    return 0;
}

static long unsigned int compute_size(const char *path) {
    total_size = 0;
    nftw(path, sum_size, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    return total_size;
}

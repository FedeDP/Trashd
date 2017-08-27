#include "../inc/utils.h"
#include <libgen.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <mntent.h>

static int sum_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static long unsigned int compute_size(const char *path);

char *my_basename(char *buff, int size, const char *str) {
    strncpy(buff, str, size);
    return basename(buff);
}

void update_directorysizes(const char *name, const char *fullp, int index) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash[index].trash_path);
    FILE *f = fopen(p, "a");
    if (f) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        fprintf(f, "%lu %lu %s\n", compute_size(fullp), tv.tv_sec, name);
        fclose(f);
    }
}

int update_info(const char *oldpath, const char *newname, int index) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/%s.trashinfo", trash[index].info_path, newname);
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

/*
 * Given a fullpath of a file, checks if it is a directory.
 * Then, searches for it in correct directorysizes file, 
 * and remove its line.
 */
void remove_line_from_directorysizes(const char *path, int index) {
    struct stat sb = {0};
    stat(path, &sb);
    if (!S_ISDIR(sb.st_mode)) {
        return;
    }
    
    int ok = 0;
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash[index].trash_path);
    FILE *f = fopen(p, "r");
    if (f) {
        char ptmp[PATH_MAX + 1] = {0};
        snprintf(ptmp, PATH_MAX, "%s/directorysizes.tmp", trash[index].trash_path);
        FILE *ftmp = fopen(ptmp, "w");
        
        if (ftmp) {
            ok = 1;
            char name[NAME_MAX + 1] = {0};
            unsigned long int t, size;
            while (!feof(f)) {
                fscanf(f, "%lu %lu %255s\n", &size, &t, name);
                if (strcmp(name, strrchr(path, '/') + 1)) {
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

/*
 * Callback function for nftw
 */
static int sum_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    total_size += sb->st_blocks * 512;
    return 0;
}

/*
 * Uses nftw to compute size of directories
 * to be cached inside directorysizes file.
 */
static long unsigned int compute_size(const char *path) {
    total_size = 0;
    nftw(path, sum_size, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    return total_size;
}

/*
 * Given a path, compute its mountpoint 
 * then searches for it inside struct trash_dir *trash array.
 * Returns found idx
 */
int get_correct_topdir_idx(const char *path) {
    int ret = -1;
    FILE *aFile = NULL;
    struct stat info;
    
    stat(path, &info);
    
    struct udev_device *dev = udev_device_new_from_devnum(udev, 'b', info.st_dev);
    if (dev) {
        const char* node  = udev_device_get_devnode(dev);
    
        struct mntent *ent;
    
        aFile = setmntent("/proc/mounts", "r");
        if (aFile == NULL) {
            perror("setmntent");
        } else {
            while ((ent = getmntent(aFile))) {
                if (!strcmp(ent->mnt_fsname, node)) {
                    break;
                }
            }
    
            for (int i = 0; i < num_topdir && ret == -1; i++) {
                if (!strcmp(trash[i].root_dir, ent->mnt_dir)) {
                    ret = i;
                }
            }
            endmntent(aFile);
        }
        udev_device_unref(dev);
    }
    return ret;
}


/*
 * Returns idx of struct trash_dir *trash
 * where trash[].wd = wd
 */
int get_idx_from_wd(const int wd) {
    int ret = -1;
    for (int i = 0; i < num_topdir && ret == -1; i++) {
        if (wd == trash[i].inot_wd) {
            ret = i;
        }
    }
    return ret;
}

void destroy_trash(void) {
    for (int i = 0; i < num_topdir; i++) {
        inotify_rm_watch(inot_fd, trash[i].inot_wd);
        if (trash[i].slot) {
            sd_bus_slot_unref(trash[i].slot);
        }
    }
    free(trash);
}

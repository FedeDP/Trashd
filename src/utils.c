#include "../inc/utils.h"
#include <libgen.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <pwd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <mntent.h>

static int create_if_needed(const char *name, const int mode);
static int sum_size(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
static long unsigned int compute_size(const char *path);

char *my_basename(char *buff, int size, const char *str) {
    strncpy(buff, str, size);
    return basename(buff);
}

/* 
 * Initializes needed directories.
 * Returns error (-1) if it cannot create needed dirs.
 */
int init_trash(const char *root, const char *dev_path) {
    struct trash_dir *tmp = NULL;
    tmp = realloc(trash, (++num_topdir) * sizeof(struct trash_dir));
    if (tmp) {
        trash = tmp;
        strncpy(trash[num_topdir - 1].dev_path, dev_path, PATH_MAX);
        /* Init trash path to root */
        strncpy(trash[num_topdir - 1].trash_path, root, PATH_MAX);
    } else {
        free(trash);
        num_topdir--;
        return -1;
    }
    
    /* if needed, create $XDG_DATA_HOME/Trash dir */
    if (!create_if_needed("Trash", S_IFDIR)) {
        /* Store correct path to trash dir */
        strncat(trash[num_topdir - 1].trash_path, "Trash", PATH_MAX - strlen(trash[num_topdir - 1].trash_path));
        /* Create if needed info and files subdir */
        if (!create_if_needed("info", S_IFDIR) && !create_if_needed("files", S_IFDIR)) {
            snprintf(trash[num_topdir - 1].info_path, PATH_MAX, "%s/info", trash[num_topdir - 1].trash_path);
            snprintf(trash[num_topdir - 1].files_path, PATH_MAX, "%s/files", trash[num_topdir - 1].trash_path);
            
            /* Cache number of trashed files */
            glob_t glob_result = {0};
            char glob_patt[PATH_MAX + 1];
            snprintf(glob_patt, PATH_MAX, "%s/*", trash[num_topdir - 1].files_path);
            glob(glob_patt, 0, NULL, &glob_result);
            trash[num_topdir - 1].num_trashed += glob_result.gl_pathc;
            globfree(&glob_result);
            
            return create_if_needed("directorysizes", S_IFREG);
        }
    }
    return -1;
}

int init_local_trash(void) {
    int ret = -1;
    char home_trash[PATH_MAX + 1] = {0};
    
    if (getenv("XDG_DATA_HOME")) {
        strncpy(home_trash, getenv("XDG_DATA_HOME"), PATH_MAX);
    } else {
        snprintf(home_trash, PATH_MAX, "%s/.local/share/", getpwuid(getuid())->pw_dir);
    }
    struct stat info;
    
    stat(home_trash, &info);
    struct udev_device *dev = udev_device_new_from_devnum(udev, 'b', info.st_dev);
    if (dev) {
        const char *node  = udev_device_get_devnode(dev);
        init_trash(home_trash, node);
        udev_device_unref(dev);
    }
    
    return ret;
}

/*
 * Checks if desired file/folders are available, otherwise
 * create them (depending on mode bit).
 */
static int create_if_needed(const char *name, const int mode) {
    struct stat sb = {0};
    char path[PATH_MAX + 1] = {0};
    
    snprintf(path, PATH_MAX, "%s/%s", trash[num_topdir - 1].trash_path, name);
    /* Check if path already exists and its mode is correct */
    if (stat(path, &sb) == 0) {
        if ((sb.st_mode & S_IFMT) != mode) {
            return -1;
        }
        return 0;
    }
    
    /* Create it if needed */
    if (mode == S_IFDIR) {
        return mkdir(path, 0700);
    }
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1) {
        return -1;
    }
    close(fd);
    return 0;
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
    struct stat info;
    
    stat(path, &info);
    struct udev_device *dev = udev_device_new_from_devnum(udev, 'b', info.st_dev);
    if (dev) {
        const char *node  = udev_device_get_devnode(dev);
        for (int i = 0; i < num_topdir && ret == -1; i++) {
            if (!strcmp(trash[i].dev_path, node)) {
                ret = i;
            }
        }
        udev_device_unref(dev);
    }
    return ret;
}

char *get_mountpoint(const char *dev_path) {
    struct mntent *part;
    FILE *mtab;
    char *ret = NULL;
    
    if (!(mtab = setmntent("/proc/mounts", "r"))) {
        fprintf(stderr, "%s\n", strerror(errno));
        return ret;
    }
    while ((part = getmntent(mtab))) {
        if ((part->mnt_fsname) && (!strcmp(part->mnt_fsname, dev_path))) {
            ret = strdup(part->mnt_dir);
            break;
        }
    }
    endmntent(mtab);
    return ret;
}

/*
 * Returns idx of struct trash_dir *trash
 * where trash[].wd = wd
 */
int get_idx_from_wd(const int wd) {
    for (int i = 0; i < num_topdir; i++) {
        if (wd == trash[i].inot_wd) {
            return i;
        }
    }
    return -1;
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

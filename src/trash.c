#include "../inc/trash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static int create_if_needed(const char *name, const int mode);
static void load_dirs_cached_size(int index);

/* 
 * Initializes needed directories.
 * Returns error (-1) if it cannot create needed dirs.
 */
int init_trash(const char *path, const char *root) {
    struct trash_dir *tmp = NULL;
    tmp = realloc(trash, (++num_topdir) * sizeof(struct trash_dir));
    if (tmp) {
        trash = tmp;
        strncpy(trash[num_topdir - 1].root_dir, root, PATH_MAX);
        strncpy(trash[num_topdir - 1].trash_path, path, PATH_MAX);
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

int method_trash(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    const char *path = NULL;
    int r;
    
    /* Reply with array response */
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    
    /* Read the parameters */
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        while (sd_bus_message_read(m, "s", &path) > 0 && r != -errno) {
            if (!strchr(path, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be absolute.");
                r = -EINVAL;
                break;
            }
            int i = get_correct_topdir_idx(path);
            if (i == -1) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Could not locate topdir.");
                r = -ENXIO;
                break;
            }

            char p[PATH_MAX + 1] = {0}, trashed_p[PATH_MAX + 1] = {0};
            char *str = my_basename(p, PATH_MAX, path);
            snprintf(trashed_p, PATH_MAX, "%s/%s", trash[i].files_path, str);
            int len = strlen(trashed_p);
            int num = 1;
            while (access(trashed_p, F_OK) == 0) {
                sprintf(trashed_p + len, " (%d)", num);
                num++;
            }
            if (rename(path, trashed_p) == -1) {
                sd_bus_error_set_errno(ret_error, errno);
                r = -errno;
            } else {
                str = my_basename(p, PATH_MAX, trashed_p);
                if (update_info(path, str, i)) {
                    sd_bus_error_set_errno(ret_error, errno);
                    r = -errno;
                } else {
                    struct stat sb = {0};
                    stat(trashed_p, &sb);
                    if (S_ISDIR(sb.st_mode)) {
                        update_directorysizes(str, trashed_p, i);
                    }
                    sd_bus_message_append(reply, "s", trashed_p);
                }
            }
        }
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    sd_bus_message_close_container(reply);
    if (!sd_bus_error_is_set(ret_error)) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return r;
}

int method_list(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    /* Reply with array response */
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            sd_bus_message_append(reply, "s", gl.gl_pathv[i]);
        }
        globfree(&gl);
    }
    
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    }
    sd_bus_message_unref(reply);
    return r;
}

static void load_dirs_cached_size(int index) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash[index].trash_path);
    FILE *f = fopen(p, "r");
    if (f) {
        char name[NAME_MAX + 1] = {0};
        unsigned long int t, size;
        while (!feof(f)) {
            fscanf(f, "%lu %lu %s\n", &size, &t, name);
            total_size += size;
        }
        fclose(f);
    }
}

int method_size(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    for (int j = 0; j < num_topdir; j++) {
        char glob_patt[PATH_MAX + 1] = {0};
        glob_t gl = {0};
        
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
        total_size = 0;
        for (int i = 0; i < gl.gl_pathc; i++) {
            struct stat sb = {0};
            stat(gl.gl_pathv[i], &sb);
            if (!S_ISDIR(sb.st_mode)) {
                total_size += sb.st_blocks * 512;
            }
        }
        globfree(&gl);
        load_dirs_cached_size(j);
    }
    return sd_bus_reply_method_return(m, "t", total_size);
}

int method_length(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int length = 0;
    for (int i = 0; i < num_topdir; i++) {
        length += trash[i].num_trashed;
    }
    return sd_bus_reply_method_return(m, "u", length);
}

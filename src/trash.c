#include "../inc/trash.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>

static int create_if_needed(const char *name, const int mode);
static void load_dirs_cached_size(void);

/* 
 * Initializes needed directories.
 * Returns error (-1) if it cannot create needed dirs.
 */
int init_trash(void) {
    if (getenv("XDG_DATA_HOME")) {
        strncpy(trash_path, getenv("XDG_DATA_HOME"), PATH_MAX);
    } else {
        snprintf(trash_path, PATH_MAX, "%s/.local/share/", getpwuid(getuid())->pw_dir);
    }
    /* if needed, create $XDG_DATA_HOME/Trash dir */
    if (!create_if_needed("Trash", S_IFDIR)) {
        /* Store correct path to trash dir */
        strncat(trash_path, "Trash", PATH_MAX - strlen(trash_path));
        /* Create if needed info and files subdir */
        if (!create_if_needed("info", S_IFDIR) && !create_if_needed("files", S_IFDIR)) {
            snprintf(info_path, PATH_MAX, "%s/info", trash_path);
            snprintf(files_path, PATH_MAX, "%s/files", trash_path);
            
            /* Cache number of trashed files */
            glob_t glob_result = {0};
            char glob_patt[PATH_MAX + 1];
            snprintf(glob_patt, PATH_MAX, "%s/*", files_path);
            glob(glob_patt, 0, NULL, &glob_result);
            number_trashed_files = glob_result.gl_pathc;
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
    
    snprintf(path, PATH_MAX, "%s/%s", trash_path, name);
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
    char (*trashed_p)[PATH_MAX + 1] = NULL;
    int r, i = 0;
    
    /* Read the parameters */
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        while (sd_bus_message_read(m, "s", &path) > 0 && r != -errno) {
            if (!strchr(path, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be absolute.");
                return -EINVAL;
            }
            i++;
            char (*tmp)[PATH_MAX + 1] = realloc(trashed_p, (PATH_MAX + 1) * i);
            if (tmp) {
                trashed_p = tmp;
                char p[PATH_MAX + 1];
                char *str = my_basename(p, PATH_MAX, path);
                snprintf(trashed_p[i - 1], PATH_MAX, "%s/%s", files_path, str);
                int len = strlen(trashed_p[i - 1]);
                int num = 1;
                while (access(trashed_p[i - 1], F_OK) == 0) {
                    sprintf(trashed_p[i - 1] + len, " (%d)", num);
                    num++;
                }
                if (rename(path, trashed_p[i - 1]) == -1) {
                    r = -errno;
                } else {
                    str = my_basename(p, PATH_MAX, trashed_p[i - 1]);
                    if (update_info(path, str)) {
                        r = -errno;
                    } else {
                        struct stat sb = {0};
                        stat(trashed_p[i - 1], &sb);
                        if (S_ISDIR(sb.st_mode)) {
                            update_directorysizes(str, trashed_p[i - 1]);
                        }
                    }
                }
            } else {
                r = -errno;
            }
        }
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    if (r == -errno) {
        sd_bus_error_set_errno(ret_error, errno);
    } else {
        /* Reply with array response */
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        for (int j = 0; j < i; j++) {
            char p[PATH_MAX + 1];
            char *str = my_basename(p, PATH_MAX, trashed_p[j]);
            sd_bus_message_append(reply, "s", str);
        }
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
    }
    
    free(trashed_p);
    return r;
}

int method_list(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    glob_t gl = {0};
    char glob_patt[PATH_MAX + 1] = {0};
    
    snprintf(glob_patt, PATH_MAX, "%s/*", files_path);
    int ret = glob(glob_patt, GLOB_MARK, NULL, &gl);
    if (ret == GLOB_NOSPACE || ret == GLOB_ABORTED) {
        sd_bus_error_set_errno(ret_error, errno);
        return -errno;
    }
    
    /* Reply with array response */
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    for (int i = 0; i < gl.gl_pathc; i++) {
        char p[PATH_MAX + 1];
        char *str = my_basename(p, PATH_MAX, gl.gl_pathv[i]);
        sd_bus_message_append(reply, "s", str);
    }
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    }
    sd_bus_message_unref(reply);
    globfree(&gl);
    return r;
}

static void load_dirs_cached_size(void) {
    char p[PATH_MAX + 1] = {0};
    snprintf(p, PATH_MAX, "%s/directorysizes", trash_path);
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
    char glob_patt[PATH_MAX + 1] = {0};
    glob_t gl = {0};
    
    snprintf(glob_patt, PATH_MAX, "%s/*", files_path);
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
    load_dirs_cached_size();
    return sd_bus_reply_method_return(m, "t", total_size);
}

int method_length(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    return sd_bus_reply_method_return(m, "u", number_trashed_files);
}

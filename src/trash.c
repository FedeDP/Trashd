#include "../inc/trash.h"

static void load_dirs_cached_size(int index);
static void find_trash_date(const char *path, int idx, char *date, size_t size);

int method_trash(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    const char *path = NULL;
    int r, size = 0;
    
    /* Read the parameters */
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        /* Reply with array response */
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        
        while (sd_bus_message_read(m, "s", &path) > 0) {
            int i = get_correct_topdir_idx(path);
            if (i == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
                continue;
            }
            if (!strncmp(path, trash[i].files_path, strlen(trash[i].files_path))) {
                fprintf(stderr, "Only not trashed files can be trashed: %s\n", trash[i].files_path);
                continue;
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
            if (rename(path, trashed_p) == 0) {
                str = my_basename(p, PATH_MAX, trashed_p);
                if (update_info(path, str, i) == 0) {
                    struct stat sb = {0};
                    stat(trashed_p, &sb);
                    if (S_ISDIR(sb.st_mode)) {
                        update_directorysizes(str, trashed_p, i);
                    }
                    sd_bus_message_append(reply, "s", trashed_p);
                    size++;
                } else {
                    fprintf(stderr, "%s\n", strerror(errno));
                }
            } else {
                fprintf(stderr, "%s\n", strerror(errno));
            }
        }
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
        return r;
    }
    
    sd_bus_message_close_container(reply);
    if (size > 0) {
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "No file could be trashed."); 
        r = -ENOENT;
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
            if (fscanf(f, "%lu %lu %s\n", &size, &t, name) == 3) {
                total_size += size;
            }
        }
        fclose(f);
    }
}

int method_size(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    total_size = 0;
    for (int j = 0; j < num_topdir; j++) {
        char glob_patt[PATH_MAX + 1] = {0};
        glob_t gl = {0};
        
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
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
        glob_t glob_result = {0};
        char glob_patt[PATH_MAX + 1];
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[i].files_path);
        glob(glob_patt, 0, NULL, &glob_result);
        length += glob_result.gl_pathc;
        globfree(&glob_result);
    }
    return sd_bus_reply_method_return(m, "u", length);
}

int method_trashdate(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    const char *path = NULL;

    int r = sd_bus_message_read(m, "s", &path);
    if (r > 0 && strchr(path, '/')) {
        int j = get_correct_topdir_idx(path);
        if (j == -1) {
            sd_bus_error_set_errno(ret_error, ENXIO);
            return -ENXIO;
        }
        if (strncmp(path, trash[j].files_path, strlen(trash[j].files_path))) {
            fprintf(stderr, "Only trashed files can show trash date: %s\n", trash[j].files_path);
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "File is not trashed.");
            return -EINVAL;
        }
        char date[50] = {0};
        find_trash_date(path, j, date, sizeof(date));
        if (!strlen(date)) {
            sd_bus_error_set_errno(ret_error, ENOENT);
            return -ENOENT;
        }
        return sd_bus_reply_method_return(m, "s", date);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "Error parsing argument."); 
        return -EINVAL;
    }
}

static void find_trash_date(const char *path, int idx, char *date, size_t size) {
    char p[PATH_MAX + 1] = {0}, info_p[PATH_MAX + 1] = {0};
    char *str = my_basename(p, PATH_MAX, path);
    
    snprintf(info_p, PATH_MAX, "%s/%s.trashinfo", trash[idx].info_path, str);
    FILE *f = fopen(info_p, "r");
    if (f) {
        fscanf(f, "%*[^\n]\n"); // jump first line
        fscanf(f, "%*[^\n]\n"); // jump second line
        
        char format[64] = {0};
        snprintf(format, sizeof(format), "DeletionDate=%%%ds\n", (int)(size));
        
        fscanf(f, format, date);
        fclose(f);
    }
}

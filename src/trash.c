#include "../inc/trash.h"

static void load_dirs_cached_size(int index);
static void find_trash_date(const char *path, int idx, char *date, size_t size);

int method_trash(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        int size = 0;
        sd_bus_message *reply = NULL;
        const char *path = NULL;
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        
        while (sd_bus_message_read(m, "s", &path) > 0) {
            char *err = NULL, trashed_p[PATH_MAX + 1] = {0};
            
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            int i = get_correct_topdir_idx(path);
            if (i == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
                err = strdup("Could not locate topdir.");
            } else if (!strncmp(path, trash[i].files_path, strlen(trash[i].files_path))) {
                fprintf(stderr, "Only not trashed files can be trashed: %s\n", trash[i].files_path);
                err = strdup("Only not trashed files can be trashed.");
            } else {
                char p[PATH_MAX + 1] = {0};
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
                        if (is_dir(trashed_p)) {
                            update_directorysizes(str, trashed_p, i);
                        }
                        size++;
                    } else {
                        fprintf(stderr, "%s\n", strerror(errno));
                        err = strdup(strerror(errno));
                    }
                } else {
                    fprintf(stderr, "%s\n", strerror(errno));
                    err = strdup(strerror(errno));
                }
            }
            sd_bus_message_append(reply, "sbs", path, !err, !err ? trashed_p : err);
            sd_bus_message_close_container(reply);
            free(err);
        }
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        if (size) {
            sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
        }
        sd_bus_message_unref(reply);
        sd_bus_message_exit_container(m);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
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
        glob(glob_patt, GLOB_MARK | GLOB_NOSORT, NULL, &gl);
    
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
            if (fscanf(f, "%lu %lu %255s\n", &size, &t, name) == 3) {
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
        glob(glob_patt, GLOB_NOSORT, NULL, &gl);
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
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *path = NULL;
        sd_bus_message *reply = NULL;
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        
        while (sd_bus_message_read(m, "s", &path) > 0) {
            char *err = NULL;
            char date[50] = {0};
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            
            int j = get_correct_topdir_idx(path);
            if (j == -1) {
                fprintf(stderr, "Could not locate %s topdir.\n", path);
                err = strdup("Could not locate topdir.");
            } else if (strncmp(path, trash[j].files_path, strlen(trash[j].files_path))) {
                fprintf(stderr, "Only trashed files can show trash date: %s\n", trash[j].files_path);
                err = strdup("File is not trashed.");
            } else {
                find_trash_date(path, j, date, sizeof(date));
                if (!strlen(date)) {
                    err = strdup("Could not locate trash date.");
                }
            }
            sd_bus_message_append(reply, "sbs", path, !err, !err ? date : err);
            sd_bus_message_close_container(reply);
            free(err);
        }
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_message_unref(reply);
        sd_bus_message_exit_container(m);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
    return r;
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

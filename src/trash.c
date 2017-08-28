#include "../inc/trash.h"

static void load_dirs_cached_size(int index);

int method_trash(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    const char *path = NULL;
    int r;
    
    /* Read the parameters */
    r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        /* Reply with array response */
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        
        while (sd_bus_message_read(m, "s", &path) > 0 && r != -errno) {
            if (!strchr(path, '/')) {
                fprintf(stderr, "Path must be absolute: %s\n", path);
                continue;
            }
            int i = get_correct_topdir_idx(path);
            if (i == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
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
    r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return r;
}

int method_listall(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
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

int method_list(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    const char *dev_path = NULL;
    
    int r = sd_bus_message_read(m, "s", &dev_path);
    if (r > 0) {
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        
        int idx = get_idx_from_devpath(dev_path);
        if (idx != -1) {
            glob_t gl = {0};
            char glob_patt[PATH_MAX + 1] = {0};
                
            snprintf(glob_patt, PATH_MAX, "%s/*", trash[idx].files_path);
            glob(glob_patt, GLOB_MARK, NULL, &gl);
                
            for (int i = 0; i < gl.gl_pathc; i++) {
                sd_bus_message_append(reply, "s", gl.gl_pathv[i]);
            }
            globfree(&gl);
        } else {
            fprintf(stderr, "Could not locate devpath: %s\n", dev_path);
            sd_bus_error_set_errno(ret_error, -ENODEV);
        }
        sd_bus_message_close_container(reply);
        if (!sd_bus_error_is_set(ret_error)) {
            int r = sd_bus_send(NULL, reply, NULL);
            if (r < 0) {
                fprintf(stderr, "%s\n", strerror(-r));
            }
        }
        sd_bus_message_unref(reply);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
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

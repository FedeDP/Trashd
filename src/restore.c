#include "../inc/restore.h"

static int restore(const char *path, sd_bus_message *reply, int idx);

int method_restore(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *path = NULL;
        char restore_p[PATH_MAX + 1] = {0};
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        while (sd_bus_message_read(m, "s", &path) > 0) {
            int j = get_correct_topdir_idx(path);
            if (j == -1) {
                fprintf(stderr, "%s\n", strerror(ENXIO));
            } else {
                if (strncmp(path, trash[j].files_path, strlen(trash[j].files_path))) {
                    fprintf(stderr, "Only trashed files can be restored: %s\n", trash[j].files_path);
                    continue;
                }
                
                char p[PATH_MAX + 1] = {0};
                char *s = my_basename(p, PATH_MAX, path);
                snprintf(restore_p, PATH_MAX, "%s/%s.trashinfo", trash[j].info_path, s);
                int ret = restore(restore_p, reply, j);
                if (ret) {
                    fprintf(stderr, "%s\n", strerror(ret));
                } else {
                    size++;
                }
            }
        }
        sd_bus_message_close_container(reply);
    } else {
        sd_bus_error_set_errno(ret_error, r);
        return r;
    }
    
    if (!sd_bus_error_is_set(ret_error)) {
        if (size > 0) {
            r = sd_bus_send(NULL, reply, NULL);
            sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
        } else {
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "No file could be restored."); 
            r = -ENOENT;
        }
    }
    sd_bus_message_unref(reply);
    return r;
}

int method_restore_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].info_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            int idx = get_correct_topdir_idx(gl.gl_pathv[i]);
            if (idx == -1) {
                fprintf(stderr, "%s\n", strerror(ENXIO));
            } else {
                int ret = restore(gl.gl_pathv[i], reply, idx);
                if (ret) {
                    fprintf(stderr, "%s\n", strerror(ret));
                } else {
                    size++;
                }
            }
        }
        globfree(&gl);
    }
    
    sd_bus_message_close_container(reply);
    int r;
    if (size > 0) {
        r = sd_bus_send(NULL, reply, NULL);
        sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
    } else {
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "No file could be restored."); 
        r = -ENOENT;
    }
    sd_bus_message_unref(reply);
    return r;
}

static int restore(const char *path, sd_bus_message *reply, int idx) {
    int ret = 0;
    
    char p[PATH_MAX + 1] = {0};
    char *s = my_basename(p, PATH_MAX, path);
    char fullpath[PATH_MAX + 1] = {0};

    snprintf(fullpath, PATH_MAX, "%s/%s", trash[idx].files_path, s);
        
    /* Remove .trashinfo extension */
    char *ptr = strstr(fullpath, ".trashinfo");
    if (ptr) {
        *ptr = '\0';
    }
        
    FILE *f = fopen(path, "r");
    if (f) {
        char old_path[PATH_MAX + 1] = {0};
        fscanf(f, "%*[^\n]\n"); // jump first line
        
        char format[32] = {0};
        snprintf(format, sizeof(format), "Path=%%%ds\n", PATH_MAX);

        fscanf(f, format, old_path);
        if (rename(fullpath, old_path) == -1) {
            ret = errno;
        }
        sd_bus_message_append(reply, "s", old_path);
        fclose(f);
            
        /* Remove .trashinfo file */
        remove(path);
            
        /* Remove line from directorysizes file */
        remove_line_from_directorysizes(fullpath, idx);
    } else {
        ret = errno;
    }
    return ret;
}

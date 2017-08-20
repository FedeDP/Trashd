#include "../inc/restore.h"

static int restore(char **str, int size, sd_bus_message *reply);

int method_restore(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *name = NULL;
        /* Elements to be restored */
        char **restore_p = NULL;
        int i = 0;
        while (sd_bus_message_read(m, "s", &name) > 0 && r != EINVAL) {
            i++;
            if (strchr(name, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be relative to trash dir.");
                r = EINVAL;
            } else {
                restore_p = realloc(restore_p, sizeof(char *) * i);
                restore_p[i - 1] = malloc(PATH_MAX + 1);
                snprintf(restore_p[i - 1], PATH_MAX, "%s/%s.trashinfo", info_path, name);
            }
        }
        if (r != EINVAL) {
            sd_bus_message_new_method_return(m, &reply);
            r = restore(restore_p, i, reply);
        }
        for (int j = 0; j < i; j++) {
            free(restore_p[j]);
        }
        free(restore_p);
    }
    if (r) {
        sd_bus_error_set_errno(ret_error, r);
        return -r;
    }
    r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return r;
}

int method_restore_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    glob_t gl = {0};
    char glob_patt[PATH_MAX + 1] = {0};
    
    snprintf(glob_patt, PATH_MAX, "%s/*", info_path);
    glob(glob_patt, GLOB_MARK, NULL, &gl);
    
    sd_bus_message_new_method_return(m, &reply);
    int ret = restore(gl.gl_pathv, gl.gl_pathc, reply);
    globfree(&gl);
    if (ret) {
        sd_bus_error_set_errno(ret_error, ret);
        return -ret;
    }
    ret = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return ret;
}

static int restore(char **str, int size, sd_bus_message *reply) {
    int ret = 0;
    
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    for (int i = 0; i < size && !ret; i++) {
        char p[PATH_MAX + 1] = {0};
        char *s = my_basename(p, PATH_MAX, str[i]);
        char fullpath[PATH_MAX + 1] = {0};
        snprintf(fullpath, PATH_MAX, "%s/%s", files_path, s);
        /* Remove .trashinfo extension */
        char *ptr = strstr(fullpath, ".trashinfo");
        if (ptr) {
            *ptr = '\0';
        }
        
        FILE *f = fopen(str[i], "r");
        if (f) {
            char old_path[PATH_MAX + 1] = {0};
            fscanf(f, "%*[^\n]\n"); // jump first line
            fscanf(f, "Path=%s\n", old_path); // read origin path
            if (rename(fullpath, old_path) == -1) {
                ret = errno;
            }
            sd_bus_message_append(reply, "s", old_path);
            fclose(f);
            
            /* Remove .trashinfo file */
            remove(str[i]);
            
            /* Remove line from directorysizes file */
            s = my_basename(p, PATH_MAX, fullpath);
            remove_line_from_directorysizes(s);
        } else {
            ret = errno;
        }
    }
    sd_bus_message_close_container(reply);
    return ret;
}

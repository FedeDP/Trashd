#include "../inc/restore.h"

static int restore(char **str, int size, sd_bus_message *reply);

int method_restore(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *path = NULL;
        /* Elements to be restored */
        char **restore_p = NULL;
        int i = 0;
        while (sd_bus_message_read(m, "s", &path) > 0 && r != EINVAL) {
            i++;
            if (!strchr(path, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be absolute.");
                r = -EINVAL;
                break;
            }
            restore_p = realloc(restore_p, sizeof(char *) * i);
            restore_p[i - 1] = malloc(PATH_MAX + 1);
            int j = get_correct_topdir_idx(path);
            char p[PATH_MAX + 1] = {0};
            char *s = my_basename(p, PATH_MAX, path);
            snprintf(restore_p[i - 1], PATH_MAX, "%s/%s.trashinfo", trash[j].info_path, s);
        }
        if (r != -EINVAL) {
            sd_bus_message_new_method_return(m, &reply);
            sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
            r = restore(restore_p, i, reply);
            if (r == errno) {
                sd_bus_error_set_errno(ret_error, r);
            }
        }
        for (int j = 0; j < i; j++) {
            free(restore_p[j]);
        }
        free(restore_p);
        sd_bus_message_close_container(reply);
    } else {
        sd_bus_error_set_errno(ret_error, r);
    }
    
    if (!sd_bus_error_is_set(ret_error)) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    if (reply) {
        sd_bus_message_unref(reply);
    }
    return r;
}

int method_restore_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int ret = 0;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    
    for (int j = 0; j < num_topdir && ret == 0; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].info_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
    
        ret = restore(gl.gl_pathv, gl.gl_pathc, reply);
        globfree(&gl);
        if (ret) {
            sd_bus_error_set_errno(ret_error, ret);
        }
    }
    
    sd_bus_message_close_container(reply);
    if (ret == 0) {
        ret = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return ret;
}

static int restore(char **str, int size, sd_bus_message *reply) {
    int ret = 0;
    
    for (int i = 0; i < size && !ret; i++) {
        char p[PATH_MAX + 1] = {0};
        char *s = my_basename(p, PATH_MAX, str[i]);
        char fullpath[PATH_MAX + 1] = {0};
        int index = get_correct_topdir_idx(str[i]);
        snprintf(fullpath, PATH_MAX, "%s/%s", trash[index].files_path, s);
        
        /* Remove .trashinfo extension */
        char *ptr = strstr(fullpath, ".trashinfo");
        if (ptr) {
            *ptr = '\0';
        }
        
        FILE *f = fopen(str[i], "r");
        if (f) {
            char old_path[PATH_MAX + 1] = {0};
            fscanf(f, "%*[^\n]\n"); // jump first line
            fscanf(f, "Path=%4096s\n", old_path); // read origin path
            if (rename(fullpath, old_path) == -1) {
                ret = errno;
            }
            sd_bus_message_append(reply, "s", old_path);
            fclose(f);
            
            /* Remove .trashinfo file */
            remove(str[i]);
            
            /* Remove line from directorysizes file */
            remove_line_from_directorysizes(fullpath, index);
        } else {
            ret = errno;
        }
    }
    
    return ret;
}

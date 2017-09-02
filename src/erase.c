#include "../inc/erase.h"

static int rmrf(const char *path, int index);
static int recursive_remove(const char *path, const struct stat *sb, 
                            int typeflag, struct FTW *ftwbuf);

int method_erase(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    /* Read the parameters */
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *path = NULL;
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        while (sd_bus_message_read(m, "s", &path) > 0) {
            if (!strchr(path, '/')) {
                fprintf(stderr, "Path must be absolute: %s\n", path);
                continue;
            }
            int idx = get_correct_topdir_idx(path);
            if (idx == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
                continue;
            }
            if (rmrf(path, idx) == 0) {
                sd_bus_message_append(reply, "s", path);
                size++;
            } else {
                fprintf(stderr, "%s\n", strerror(errno));
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
            sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "No file could be erased."); 
            r = -ENOENT;
        }
    }
    sd_bus_message_unref(reply);
    return r;
}

int method_erase_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            if (rmrf(gl.gl_pathv[i], j) == 0) {
                sd_bus_message_append(reply, "s", gl.gl_pathv[i]);
                size++;
            } else {
                fprintf(stderr, "%s\n", strerror(errno));
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
        sd_bus_error_set_const(ret_error, SD_BUS_ERROR_INVALID_ARGS, "No file could be erased."); 
        r = -ENOENT;
    }
    sd_bus_message_unref(reply);
    return r;
}

static int rmrf(const char *path, int index) {
    /* remove file */
    int ret = nftw(path, recursive_remove, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    
    if (ret == 0) {
        /* remove trashinfo file */
        char p[PATH_MAX + 1] = {0}, rm_p[PATH_MAX + 1] = {0};
        char *s = my_basename(p, PATH_MAX, path);
        snprintf(rm_p, PATH_MAX, "%s/%s.trashinfo", trash[index].info_path, s);
        ret = remove(rm_p);
    }
    
    if (ret == 0) {
        /* Remove line from directorysizes file */
        remove_line_from_directorysizes(path, index);
    }
    return ret;
}

static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(path);
}

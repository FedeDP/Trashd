#include "../inc/erase.h"

static void rmrf(const char *path, int index);
static int recursive_remove(const char *path, const struct stat *sb, 
                            int typeflag, struct FTW *ftwbuf);

int method_erase(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *path = NULL;
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        while (sd_bus_message_read(m, "s", &path) > 0 && r != EINVAL) {
            if (!strchr(path, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be absolute.");
                r = -EINVAL;
                break;
            }
            rmrf(path, get_correct_topdir_idx(path));
            sd_bus_message_append(reply, "s", path);
        }
        sd_bus_message_close_container(reply);
    } else {
        sd_bus_error_set_errno(ret_error, r);
    }
    
    if (!sd_bus_error_is_set(ret_error)) {
        r = sd_bus_send(NULL, reply, NULL);
    }
    sd_bus_message_unref(reply);
    return r;
}

int method_erase_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            rmrf(gl.gl_pathv[i], j);
            sd_bus_message_append(reply, "s", gl.gl_pathv[i]);
        }
        globfree(&gl);
    }
    sd_bus_message_close_container(reply);
    int ret = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return ret;
}

static void rmrf(const char *path, int index) {
    /* remove file */
    nftw(path, recursive_remove, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    
    /* remove trashinfo file */
    char p[PATH_MAX + 1] = {0}, rm_p[PATH_MAX + 1] = {0};
    char *s = my_basename(p, PATH_MAX, path);
    snprintf(rm_p, PATH_MAX, "%s/%s.trashinfo", trash[index].info_path, s);
    remove(rm_p);
    
    /* Remove line from directorysizes file */
    remove_line_from_directorysizes(path, index);
}

static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(path);
}

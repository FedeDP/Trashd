#include "../inc/erase.h"
#include "../inc/utils.h"

static void rmrf(const char *name);
static int recursive_remove(const char *path, const struct stat *sb, 
                            int typeflag, struct FTW *ftwbuf);

int method_erase(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    
    /* Read the parameters */
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        const char *name = NULL;
        int i = 0;
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
        while (sd_bus_message_read(m, "s", &name) > 0 && r != EINVAL) {
            i++;
            if (strchr(name, '/')) {
                sd_bus_error_set_const(ret_error, SD_BUS_ERROR_FAILED, "Path must be relative to trash dir.");
                return -EINVAL;
            } else {
                rmrf(name);
                sd_bus_message_append(reply, "s", name);
            }
        }
        sd_bus_message_close_container(reply);
    }
    if (r < 0) {
        sd_bus_error_set_errno(ret_error, r);
        return r;
    }
    r = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return r;
}

int method_erase_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    glob_t gl = {0};
    char glob_patt[PATH_MAX + 1] = {0};
    
    snprintf(glob_patt, PATH_MAX, "%s/*", files_path);
    glob(glob_patt, GLOB_MARK, NULL, &gl);
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "s");
    for (int i = 0; i < gl.gl_pathc; i++) {
        char p[PATH_MAX + 1];
        char *s = my_basename(p, PATH_MAX, gl.gl_pathv[i]);
        rmrf(s);
        sd_bus_message_append(reply, "s", s);
    }
    sd_bus_message_close_container(reply);
    globfree(&gl);
    int ret = sd_bus_send(NULL, reply, NULL);
    sd_bus_message_unref(reply);
    return ret;
}

static void rmrf(const char *name) {
    char rm_p[PATH_MAX + 1] = {0};
    snprintf(rm_p, PATH_MAX, "%s/%s", files_path, name);
    /* remove file */
    nftw(rm_p, recursive_remove, 64, FTW_DEPTH | FTW_PHYS | FTW_MOUNT);
    
    /* remove trashinfo file */
    snprintf(rm_p, PATH_MAX, "%s/%s.trashinfo", info_path, name);
    remove(rm_p);
}

static int recursive_remove(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    return remove(path);
}

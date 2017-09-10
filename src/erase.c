#include "../inc/erase.h"

static int rmrf(const char *path, int index);
static int recursive_remove(const char *path, const struct stat *sb, 
                            int typeflag, struct FTW *ftwbuf);

int method_erase(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        int size = 0;
        const char *path = NULL;
        sd_bus_message *reply = NULL;
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        
        while (sd_bus_message_read(m, "s", &path) > 0) {
            char *err = NULL;
            
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            int idx = get_correct_topdir_idx(path);
            if (idx == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
                err = strdup("Could not locate topdir.");
            } else if (strncmp(path, trash[idx].files_path, strlen(trash[idx].files_path))) {
                fprintf(stderr, "Only trashed files can be erased: %s\n", trash[idx].files_path);
                err = strdup("Only trashed files can be erased.");
            } else if (rmrf(path, idx)) {
                fprintf(stderr, "%s\n", strerror(errno));
                err = strdup(strerror(errno));
            } else {
                size++;
            }
            sd_bus_message_append(reply, "sbs", path, !err, !err ? NULL : err);
            sd_bus_message_close_container(reply);
            free(err);
        }
        sd_bus_message_close_container(reply);
        r = sd_bus_send(NULL, reply, NULL);
        if (size) {
            sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
        }
        sd_bus_message_unref(reply);
    } else {
        fprintf(stderr, "Failed to parse parameters: %s\n", strerror(-r));
    }
    return r;
}

int method_erase_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
    
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_MARK | GLOB_NOSORT, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            char *err = NULL;
            
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            if (rmrf(gl.gl_pathv[i], j) == 0) {
                size++;
            } else {
                fprintf(stderr, "%s\n", strerror(errno));
                err = strdup(strerror(errno));
            }
            sd_bus_message_append(reply, "sbs", gl.gl_pathv[i], !err, !err ? NULL : err);
            sd_bus_message_close_container(reply);
            free(err);
        }
        globfree(&gl);
    }
    sd_bus_message_close_container(reply);
    int r = sd_bus_send(NULL, reply, NULL);
    if (size) {
        sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
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

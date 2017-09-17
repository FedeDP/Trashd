#include "../inc/restore.h"
#include "../inc/utils.h"

static int restore(char *filepath, int idx);

int method_restore(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r = sd_bus_message_enter_container(m, SD_BUS_TYPE_ARRAY, "s");
    if (r >= 0) {
        sd_bus_message *reply = NULL;
        int size = 0;
        const char *path = NULL;
        
        sd_bus_message_new_method_return(m, &reply);
        sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        
        while (sd_bus_message_read(m, "s", &path) > 0) {
            char *err = NULL, restored_p[PATH_MAX + 1] = {0};
            
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            
            int j = get_correct_topdir_idx(path);
            if (j == -1) {
                fprintf(stderr, "Could not locate topdir: %s\n", path);
                err = strdup("Could not locate topdir.");
            } else if (strncmp(path, trash[j].files_path, strlen(trash[j].files_path))) {
                fprintf(stderr, "Only trashed files can be restored: %s\n", trash[j].files_path);
                err = strdup("Only trashed files can be restored.");
            } else {
                strncpy(restored_p, path, PATH_MAX);
                int ret = restore(restored_p, j);
                if (ret) {
                    fprintf(stderr, "%s\n", strerror(ret));
                    err = strdup(strerror(ret));
                } else {
                    size++;
                }
            }
            sd_bus_message_append(reply, "sbs", path, !err, !err ? restored_p : err);
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

int method_restore_all(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_message *reply = NULL;
    int size = 0;
    
    sd_bus_message_new_method_return(m, &reply);
    sd_bus_message_open_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
    
    for (int j = 0; j < num_topdir; j++) {
        glob_t gl = {0};
        char glob_patt[PATH_MAX + 1] = {0};
    
        snprintf(glob_patt, PATH_MAX, "%s/*", trash[j].files_path);
        glob(glob_patt, GLOB_NOSORT, NULL, &gl);
    
        for (int i = 0; i < gl.gl_pathc; i++) {
            char *err = NULL, restored_p[PATH_MAX + 1] = {0};
            
            sd_bus_message_open_container(reply, SD_BUS_TYPE_STRUCT, "sbs");
            
            strncpy(restored_p, gl.gl_pathv[i], PATH_MAX);
            int ret = restore(restored_p, j);
            if (ret) {
                fprintf(stderr, "%s\n", strerror(ret));
                err = strdup(strerror(ret));
            } else {
                size++;
            }
            
            sd_bus_message_append(reply, "sbs", gl.gl_pathv[i], !err, !err ? restored_p : err);
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

static int restore(char *filepath, int idx) {
    int ret = 0;
    
    char p[PATH_MAX + 1] = {0}, infopath[PATH_MAX + 1] = {0};
    char *s = my_basename(p, PATH_MAX, filepath);
    snprintf(infopath, PATH_MAX, "%s/%s.trashinfo", trash[idx].info_path, s);
        
    FILE *f = fopen(infopath, "r");
    if (f) {
        int dir = is_dir(filepath);
        
        char old_path[PATH_MAX + 1] = {0};
        fscanf(f, "%*[^\n]\n"); // jump first line
        
        char format[32] = {0};
        snprintf(format, sizeof(format), "Path=%%%ds\n", PATH_MAX);

        fscanf(f, format, old_path);
        fclose(f);
        
        if (my_rename(filepath, old_path, sizeof(old_path)) == -1) {
            ret = errno;
        } else {
            /* Remove .trashinfo file */
            remove(infopath);
            
            /* Remove line from directorysizes file */
            if (dir) {
                remove_line_from_directorysizes(filepath, idx);
            }
        }
        /* Return correct restored path for this file */
        strncpy(filepath, old_path, PATH_MAX);
    } else {
        ret = errno;
    }
    return ret;
}

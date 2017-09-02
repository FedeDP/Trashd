#include "../inc/udisks.h"

static int change_callback(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void free_bus_struct(sd_bus_error *error, sd_bus_message *mess);

static sd_bus *systembus;

int init_udisks(void) {
    sd_bus_default_system(&systembus);
    
    int r = sd_bus_add_match(systembus, NULL,
                         "type='signal',"
                         "sender='org.freedesktop.UDisks2',"
                         "interface='org.freedesktop.DBus.Properties',"
                         "member='PropertiesChanged'",
                         change_callback, NULL);
    
    /* This way get_fd will return -EINVAL and poll won't poll this bus */
    if (r < 0) {
        destroy_udisks();
    }
    return r;
}

void *get_udisks_bus(void) {
    return systembus;
}

/*
 * If message received == org.freedesktop.UDisks2.Filesystem, then
 * only possible signal is "mounted status has changed".
 * So, change mounted status of device (device name = sd_bus_message_get_path(m))
 */
static int change_callback(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    int r;
    const char *path;
    const char obj[] = "org.freedesktop.UDisks2.Filesystem";
    char devpath[PATH_MAX + 1] = {0};
    
    r = sd_bus_message_read(m, "s", &path);
    if (r < 0) {
        fprintf(stderr, "%s\n", strerror(-r));
    } else {
        if (!strncmp(obj, path, strlen(obj))) {
            const char *name = sd_bus_message_get_path(m);
            snprintf(devpath, PATH_MAX, "/dev/%s", strrchr(name, '/') + 1);
            char *mp = get_mountpoint(devpath);
            if (mp) {
                /* A device has been mounted */
                init_trash(mp, devpath);
                free(mp);
            } else {
                /* Device umounted */
                int idx = get_idx_from_devpath(devpath);
                if (idx != -1) {
                    remove_trash(idx);
                }
            }
            sd_bus_emit_signal(bus, object_path, bus_interface, "TrashChanged", NULL);
        }
    }
    return 0;
}


void load_trashes(void) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *mess = NULL;
            
    int r = sd_bus_call_method(systembus,
                            "org.freedesktop.UDisks2",
                           "/org/freedesktop/UDisks2/Manager",
                           "org.freedesktop.UDisks2.Manager",
                           "GetBlockDevices",
                            &error,
                            &mess,
                           "a{sv}",
                            0);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
    } else {
        r = sd_bus_message_enter_container(mess, SD_BUS_TYPE_ARRAY, "o");
        if (r < 0) {
            fprintf(stderr, "%s\n", strerror(-r));
        } else {
            const char *obj_path;
            while ((sd_bus_message_read(mess, "o", &obj_path)) > 0) {
                char *ptr = strrchr(obj_path, '/') + 1;
                char dev_path[PATH_MAX + 1] = {0};
                snprintf(dev_path, PATH_MAX, "/dev/%s", ptr);
                char *mp = get_mountpoint(dev_path);
                char *uuid = get_uuid(dev_path);
                /* If is mounted and it is not same device as our local home trash (trash[0] is local) */
                if (mp && uuid && strcmp(uuid, trash[0].uuid)) {
                    init_trash(mp, uuid);
                }
                free(mp);
                free(uuid);
            }
        }
    }
    free_bus_struct(&error, mess);
}

int get_udisks_fd(void) {
    return sd_bus_get_fd(systembus);
}

static void free_bus_struct(sd_bus_error *error, sd_bus_message *mess) {
    if (mess) {
        sd_bus_message_unref(mess);
    }
    if (error) {
        sd_bus_error_free(error);
    }
}

void destroy_udisks(void) {
    if (systembus) {
        systembus = sd_bus_flush_close_unref(systembus);
    }
}

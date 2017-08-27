#include "../inc/udisks.h"

static void free_bus_struct(sd_bus_error *error, sd_bus_message *mess);

static sd_bus *systembus;

void init_udisks(void) {
    sd_bus_default_system(&systembus);
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
                /* If is mounted and it is not same device as our local home trash (trash[0] is local) */
                if (mp && strcmp(dev_path, trash[0].dev_path)) {
                    init_trash(mp, dev_path);
                }
                free(mp);
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
        sd_bus_flush_close_unref(systembus);
    }
}

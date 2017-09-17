#include <systemd/sd-bus.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <linux/limits.h>

#define SIZE(a) sizeof(a) / sizeof(*a)

static int on_trash_update(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
static void trash_files(int len, char to_be_trashed[][PATH_MAX + 1], int *size);
static void restore_files(int size);

static sd_bus *bus;
static const char *trashed_files[] = { "trash1.txt", "trash2.txt", "not_existent.txt" }; // last file is not existent -> gives error
static char **restored_files;

int main(void) {
    sd_bus_default_user(&bus);
    
    /* Hook our function to TrashChanged signal */
    sd_bus_add_match(bus, NULL, "type='signal', sender='org.trash.trashd', interface='org.trash.trashd', path='/org/trash/trashd', member='TrashChanged'", on_trash_update, NULL);
    
    /* Initial sd_bus_process */
    sd_bus_process(bus, NULL);
        
    /* Print initial trash list of files */
    on_trash_update(NULL, NULL, NULL);
    
    /* Create correct path to trashed_files list */
    char to_be_trashed[SIZE(trashed_files)][PATH_MAX + 1] = {{0}};
    char *cwd = getcwd(NULL, 0);
    for (int i = 0; i < SIZE(trashed_files); i++) {
        snprintf(to_be_trashed[i], PATH_MAX, "%s/%s", cwd, trashed_files[i]);
    }
    free(cwd);
        
    int size = 0;
    trash_files(SIZE(trashed_files), to_be_trashed, &size);
        
    sd_bus_wait(bus, (uint64_t) -1);
    sd_bus_process(bus, NULL);
    
    if (size > 0) {
        restore_files(size);
        for (int i = 0; i < size; i++) {
            free(restored_files[i]);
        }
        free(restored_files);
        
        sd_bus_wait(bus, (uint64_t) -1);
        sd_bus_process(bus, NULL);
    }

    sd_bus_flush_close_unref(bus);
    return 0;
}

static int on_trash_update(sd_bus_message *m, void *userdata, sd_bus_error *ret_error) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *mess = NULL;
    
    int r = sd_bus_call_method(bus,
                               "org.trash.trashd",
                               "/org/trash/trashd",
                               "org.trash.trashd",
                               "List",
                               &error,
                               &mess,
                               NULL);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
    } else {
        printf("Trash:\t[\n");
        r = sd_bus_message_enter_container(mess, SD_BUS_TYPE_ARRAY, "s");
        if (r < 0) {
            fprintf(stderr, "%s\n", strerror(-r));
        } else {
            const char *path = NULL;
            while ((sd_bus_message_read(mess, "s", &path)) > 0) {
                printf("\t\t%s\n", strrchr(path, '/') + 1);
            }
            sd_bus_message_exit_container(mess);
        }
        printf("\t]\n\n");
    }
    sd_bus_message_unref(mess);
    sd_bus_error_free(&error);
    return 0;
}

static void trash_files(int len, char to_be_trashed[][PATH_MAX + 1], int *size) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *mess = NULL, *reply = NULL;
    
    sd_bus_message_new_method_call(bus, &mess, "org.trash.trashd", "/org/trash/trashd", "org.trash.trashd", "Trash");
    sd_bus_message_open_container(mess, SD_BUS_TYPE_ARRAY, "s");
    for (int i = 0; i < len; i++) {
        sd_bus_message_append(mess, "s", to_be_trashed[i]);
    }
    sd_bus_message_close_container(mess);
    int r = sd_bus_call(bus, mess, 0, &error, &reply);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
    } else {
        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        if (r < 0) {
            fprintf(stderr, "%s\n", strerror(-r));
        } else {
            while (sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "sbs") > 0) {
                const char *path = NULL, *output = NULL;
                int ok;
                if (sd_bus_message_read(reply, "sbs", &path, &ok, &output) > 0) {
                    if (ok) {
                        // file trashed with no errors
                        printf("* Trashed %s in %s.\n", path, output);
                        char **tmp = realloc(restored_files, sizeof(char *) * ((*size) + 1));
                        if (tmp) {
                            restored_files = tmp;
                            restored_files[*size] = strdup(output);
                            (*size)++;
                        }
                    } else {
                        // file not trashed; print error
                        fprintf(stderr, "# %s\n", output);
                    }
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
            printf("\n");
        }
    }
    sd_bus_message_unref(mess);
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
}

static void restore_files(int size) {
    sd_bus_error error = SD_BUS_ERROR_NULL;
    sd_bus_message *mess = NULL, *reply = NULL;
    
    sd_bus_message_new_method_call(bus, &mess, "org.trash.trashd", "/org/trash/trashd", "org.trash.trashd", "Restore");
    sd_bus_message_open_container(mess, SD_BUS_TYPE_ARRAY, "s");
    for (int i = 0; i < size; i++) {
        sd_bus_message_append(mess, "s", restored_files[i]);
    }
    sd_bus_message_close_container(mess);
    int r = sd_bus_call(bus, mess, 0, &error, &reply);
    if (r < 0) {
        fprintf(stderr, "%s\n", error.message);
    } else {
        r = sd_bus_message_enter_container(reply, SD_BUS_TYPE_ARRAY, "(sbs)");
        if (r < 0) {
            fprintf(stderr, "%s\n", strerror(-r));
        } else {
            while (sd_bus_message_enter_container(reply, SD_BUS_TYPE_STRUCT, "sbs") > 0) {
                const char *path = NULL, *output = NULL;
                int ok;
                if (sd_bus_message_read(reply, "sbs", &path, &ok, &output) > 0) {
                    if (ok) {
                        // file restored with no errors
                        printf("* Restored %s.\n", output);
                    } else {
                        // file not restored; print error
                        fprintf(stderr, "%s\n", output);
                    }
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
            printf("\n");
        }
    }
    sd_bus_message_unref(mess);
    sd_bus_message_unref(reply);
    sd_bus_error_free(&error);
}

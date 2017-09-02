#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-bus-vtable.h>
#include <unistd.h>
#include <glob.h>
#include <linux/limits.h>
#include <ftw.h>
#include <libudev.h>

#define UUID_LEN 36

struct trash_dir {
    char trash_path[PATH_MAX + 1];          // path to trash dir
    char info_path[PATH_MAX + 1];           // subdir of trash dir
    char files_path[PATH_MAX + 1];          // subdir of trash dir
    char uuid[UUID_LEN + 1];                // UUID for this trash FS
    int num_trashed;                        // number of trashed files in this trash
};

sd_bus *bus;
int num_topdir;
long unsigned int total_size;
struct trash_dir *trash;
struct udev *udev;
static const char object_path[] = "/org/trash/trashd";
static const char bus_interface[] = "org.trash.trashd";

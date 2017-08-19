#pragma once

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <systemd/sd-bus.h>
#include <systemd/sd-bus-vtable.h>
#include <unistd.h>
#include <glob.h>
#include <linux/limits.h>

sd_bus *bus;
char trash_path[PATH_MAX + 1], info_path[PATH_MAX + 1], files_path[PATH_MAX + 1];
int number_trashed_files;

#include "commons.h"

int init_trash(void);
int method_trash(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_list(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_size(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);
int method_length(sd_bus_message *m, void *userdata, sd_bus_error *ret_error);

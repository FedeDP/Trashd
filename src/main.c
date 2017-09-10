#include <sys/signalfd.h>
#include <poll.h>
#include <signal.h>
#include "../inc/trash.h"
#include "../inc/restore.h"
#include "../inc/erase.h"
#include "../inc/udisks.h"

static void bus_cb(sd_bus *b);
static void signal_cb(void);
static void set_pollfd(void);
static void main_poll(void);
static void close_mainp(void);

enum poll_idx { BUS, SIGNAL, UDISKS, POLL_SIZE };
enum quit_codes { LEAVE_W_ERR = -1, SIGNAL_RCV = 1 };

static struct pollfd main_p[POLL_SIZE];
static int quit;

static const sd_bus_vtable trashd_vtable[] = {
    SD_BUS_VTABLE_START(0),
    /* move array of fullpath to trash */
    SD_BUS_METHOD("Trash", "as", "a(sbs)", method_trash, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Completely delete array of elements (must already be present in trash) */
    SD_BUS_METHOD("Erase", "as", "a(sbs)", method_erase, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Completely delete every trashed file */
    SD_BUS_METHOD("EraseAll", NULL, "a(sbs)", method_erase_all, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Restore array of elements to their original position */
    SD_BUS_METHOD("Restore", "as", "a(sbs)", method_restore, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Restore all trashed elements to their original position */
    SD_BUS_METHOD("RestoreAll", NULL, "a(sbs)", method_restore_all, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Get trashing date of file */
    SD_BUS_METHOD("TrashDate", "as", "a(sbs)", method_trashdate, SD_BUS_VTABLE_UNPRIVILEGED),
    /* List all files in trash */
    SD_BUS_METHOD("List", NULL, "as", method_list, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Size in bytes of trashed files */
    SD_BUS_METHOD("Size", NULL, "t", method_size, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Number of trashed files */
    SD_BUS_METHOD("Length", NULL, "u", method_length, SD_BUS_VTABLE_UNPRIVILEGED),
    /* Signal emitted when any change happens to trash */
    SD_BUS_SIGNAL("TrashChanged", NULL, 0),
    SD_BUS_VTABLE_END
};

static void bus_cb(sd_bus *b) {
    int r;
    do {
        r = sd_bus_process(b, NULL);
        if (r < 0) {
            fprintf(stderr, "Failed to process bus: %s\n", strerror(-r));
            quit = LEAVE_W_ERR;
        }
    } while (r > 0);
}

/*
 * if received an external SIGINT or SIGTERM,
 * just switch the quit flag to 1 and print to stdout.
 */
static void signal_cb(void) {
    struct signalfd_siginfo fdsi;
    ssize_t s;
    
    s = read(main_p[SIGNAL].fd, &fdsi, sizeof(struct signalfd_siginfo));
    if (s != sizeof(struct signalfd_siginfo)) {
        fprintf(stderr, "an error occurred while getting signalfd data.\n");
    }
    printf("Received signal %d. Leaving.\n", fdsi.ssi_signo);
    quit = SIGNAL_RCV;
}

static void set_pollfd(void) {
    main_p[BUS] = (struct pollfd) {
        .fd = sd_bus_get_fd(bus),
        .events = POLLIN,
    };
    
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
    sigprocmask(SIG_BLOCK, &mask, NULL);
    main_p[SIGNAL] = (struct pollfd) {
        .fd = signalfd(-1, &mask, 0),
        .events = POLLIN,
    };
    
    main_p[UDISKS] = (struct pollfd) {
        .fd = get_udisks_fd(),
        .events = POLLIN,
    };
}

/*
 * Listens on fds
 */
static void main_poll(void) {
    while (!quit) {
        int r = poll(main_p, POLL_SIZE, -1);
        
        if (r == -1 && errno != EINTR) {
            fprintf(stderr, "%s\n", strerror(errno));
            quit = LEAVE_W_ERR;
        }
        
        while (r > 0 && !quit) {
            for (int i = 0; i < POLL_SIZE; i++) {
                if (main_p[i].revents & POLLIN) {
                    switch (i) {
                    case BUS:
                        bus_cb(bus);
                        break;
                    case SIGNAL:
                        signal_cb();
                        break;
                    case UDISKS:
                        bus_cb(get_udisks_bus());
                        break;
                    }
                    r--;
                    break;
                }
            }
        }
    }
}

static void close_mainp(void) {
    for (int i = BUS; i < POLL_SIZE; i++) {
        if (main_p[i].fd > 0) {
            close(main_p[i].fd);
        }
    }
}

int main(void) {
    int r;
    
    /* Connect to the user bus */
    r = sd_bus_default_user(&bus);
    if (r < 0) {
        fprintf(stderr, "Failed to connect to user bus: %s\n", strerror(-r));
        goto finish;
    }
    
    /* Install the object */
    r = sd_bus_add_object_vtable(bus,
                                 NULL,
                                 object_path,
                                 bus_interface,
                                 trashd_vtable,
                                 NULL);
    if (r < 0) {
        fprintf(stderr, "Failed to issue method call: %s\n", strerror(-r));
        goto finish;
    }
    
    /* Take a well-known service name so that clients can find us */
    r = sd_bus_request_name(bus, bus_interface, 0);
    if (r < 0) {
        fprintf(stderr, "Failed to acquire service name: %s\n", strerror(-r));
        goto finish;
    }
    
    /* Create our udev instance */
    udev = udev_new();
    if (init_local_trash()) {
        goto finish;
    }
    /* Init udisks and topdirs support */
    if (init_udisks() >= 0) {
        load_trashes();
    }
    set_pollfd();
    
   /*
    * Need to parse initial bus messages 
    * or it'll give "Connection timed out" error
    */
    bus_cb(bus);
    main_poll();
    
finish:
    sd_bus_release_name(bus, bus_interface);
    if (bus) {
        sd_bus_flush_close_unref(bus);
    }
    destroy_udisks();
    free(trash);
    close_mainp();
    udev_unref(udev);
    return quit == LEAVE_W_ERR ? EXIT_FAILURE : EXIT_SUCCESS;
}

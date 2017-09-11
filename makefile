BINDIR = /usr/lib/trash
BINNAME = trashd
BUSCONFDIR = /etc/dbus-1/system.d/
BUSCONFNAME = org.trash.trashd.conf
BUSSERVICEDIR = /usr/share/dbus-1/services/
BUSSERVICENAME = org.trash.trashd.service
SYSTEMDSERVICE = trashd.service
SYSTEMDDIR = /usr/lib/systemd/user
EXTRADIR = Scripts
SAMPLEDIR = sample
RM = rm -f
RMDIR = rm -rf
INSTALL = install -p
INSTALL_PROGRAM = $(INSTALL) -m755
INSTALL_DATA = $(INSTALL) -m644
INSTALL_DIR = $(INSTALL) -d
SRCDIR = src/
LIBS = -lm $(shell pkg-config --libs libsystemd libudev)
CFLAGS = $(shell pkg-config --cflags libsystemd libudev) -D_GNU_SOURCE -std=c99

ifeq (,$(findstring $(MAKECMDGOALS),"clean install uninstall"))

ifneq ("$(shell pkg-config --atleast-version=221 systemd && echo yes)", "yes")
$(error systemd minimum required version 221.)
endif

endif

TRASHD_VERSION = $(shell git describe --abbrev=0 --always --tags)

all: trashd clean

debug: trashd-debug clean

sample: SRCDIR=$(SAMPLEDIR)
sample: BINNAME=sample/sample
sample: all

objects:
	@cd $(SRCDIR); $(CC) -c *.c $(CFLAGS) -O3

objects-debug:
	@cd $(SRCDIR); $(CC) -c *.c -Wall $(CFLAGS) -Wshadow -Wtype-limits -Wstrict-overflow -fno-strict-aliasing -Wno-format -g

trashd: objects
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

trashd-debug: objects-debug
	@cd $(SRCDIR); $(CC) -o ../$(BINNAME) *.o $(LIBS)

clean:
	@cd $(SRCDIR); $(RM) *.o

install:
	$(info installing bin.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BINDIR)"
	@$(INSTALL_PROGRAM) $(BINNAME) "$(DESTDIR)$(BINDIR)"

	$(info installing dbus conf file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSCONFDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSCONFNAME) "$(DESTDIR)$(BUSCONFDIR)"

	$(info installing dbus service file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(BUSSERVICEDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(BUSSERVICENAME) "$(DESTDIR)$(BUSSERVICEDIR)"
	
	$(info installing systemd service file.)
	@$(INSTALL_DIR) "$(DESTDIR)$(SYSTEMDDIR)"
	@$(INSTALL_DATA) $(EXTRADIR)/$(SYSTEMDSERVICE) "$(DESTDIR)$(SYSTEMDDIR)"

uninstall:
	$(info uninstalling bin.)
	@$(RM) "$(DESTDIR)$(BINDIR)/$(BINNAME)"

	$(info uninstalling dbus conf file.)
	@$(RM) "$(DESTDIR)$(BUSCONFDIR)/$(BUSCONFNAME)"

	$(info uninstalling dbus service file.)
	@$(RM) "$(DESTDIR)$(BUSSERVICEDIR)/$(BUSSERVICENAME)"
	
	$(info uninstalling systemd service file.)
	@$(RM) "$(DESTDIR)$(SYSTEMDDIR)/$(SYSTEMDSERVICE)"

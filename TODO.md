## TODO

### TOPDIR SUPPORT
- [x] org.freedesktop.UDisks2.Manager.GetBlockDevices() + foreach block device -> get its mountpoint. If any, it means it is mounted.
- [ ] Add a match on org.freedesktop.UDisks2.Filesystem.MountPoint property for each block device
- [x] Load and call init_trash for each mounted fs
- [ ] Check if $topdir/.Trash does exist for any external drive, otherwise try to create $topdir/.Trash-uid/ dir
- [ ] Udisks2 monitor -> at every new mounted device, init_trash on it
- [ ] Udisks2 monitor -> for every unmounted device, remove it from inotify and remove it from struct trash_dirs *trash,  destroying its sd_bus_slot.

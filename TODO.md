## TODO

### TOPDIR SUPPORT
- [x] org.freedesktop.UDisks2.Manager.GetBlockDevices() + foreach block device -> get its mountpoint. If any, it means it is mounted.
- [x] Monitor Udisks2 for mounted/unmounted device
- [x] Load and call init_trash for each mounted fs
- [ ] Check if $topdir/.Trash does exist for any external drive, otherwise try to create $topdir/.Trash-uid/ dir
- [x] Udisks2 monitor -> at every new mounted device, init_trash on it and send a TrashAdded signal with devpath
- [x] Udisks2 monitor -> for every unmounted device, remove it from inotify and remove it from struct trash_dirs *trash, Send a TrashRemoved signal too.

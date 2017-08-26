## TODO

### TOPDIR SUPPORT
- [ ] Load and call init_trash for each mounted fs
- [ ] Check if $topdir/.Trash does exist for any external drive, otherwise try to create $topdir/.Trash-uid/ dir
- [ ] Udisks2 monitor -> at every new mounted device, init_trash on it and inotify_add_watch on its $topdir
- [ ] Udisks2 monitor -> for every unmounted device, remove it from inotify and remove it from struct trash_dirs *trash

#### Methods porting to topdir support
- [x] Trash
- [x] Erase
- [x] EraseAll
- [x] Restore -> fix
- [x] RestoreAll
- [x] List
- [x] Size
- [x] Length

- [x] What happens if a new topdir is discovered (eg a new usb external drive has been attached) and it already has a trashed file with same name as another trashed one? Possible fix: use fullpath always (in both List, Erase, Restore, Trash (as reply))

- [x] udev to be globally available (do not create it every time get_correct_topdir_idx is called)

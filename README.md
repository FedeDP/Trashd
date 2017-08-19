# Trashd

Linux bus user service to provide an implementation of freedesktop trash spec: https://specifications.freedesktop.org/trash-spec/trashspec-latest.html.  
It aims to be used by file managers/DE as a DE agnostic, generic solution to manage trash.  

## Build
It only depends on libsystemd (systemd/sd-bus.h).  

    $ make
    # make install

## Interface
### Methods
| Name | IN | IN values | OUT | OUT values |
|-|:-:|-|:-:|-|
| Trash | as | <ul><li>Array of fullpaths to be moved to trash</li></ul> | as | Trashed files |
| Erase | as | <ul><li>Array of relative-to-trash paths to be unlinked from fs</li></ul> | as | Erased files |
| EraseAll | | | as | Erased files |
| Restore | as | <ul><li>Array of relative-to-trash paths to be restored</li></ul> | as | Restored positions for every file |
| RestoreAll | | | as | Restored positions for every file |
| List | | | as | List of trashed files |
| Size | | | t | Current trash size in bytes |
| Length | | | u | Current number of trashed elements |

### Signals
| Name | When | OUT | OUT values |
|-|:-:|-|:-:|
| Trashed | A new file has been moved to trash | s | Name of trashed file |
| Erased | A file has been completely erased | s | Name of erased file |
| Restored | A file has been restored | s | Name of restored file |

Please note that to eg trash a directory, you should pass fullpath to the directory **without** trailing slash.

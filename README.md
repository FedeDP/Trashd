# Trashd

Linux bus user service to provide an implementation of freedesktop trash spec: https://specifications.freedesktop.org/trash-spec/trashspec-latest.html.  
It aims to be used by file managers/DE as a DE agnostic, generic solution to manage trash.  

## Build
It only depends on libsystemd (systemd/sd-bus.h).  

    $ make
    # make install
    
## Runtime dep
Top directories trashing support requires UDisks2 available at runtime. This is needed to react to newly mounted filesystems.  
If UDisks2 is not available, topdir support will not work, ie: Trashd will only support home trash.  

## Interface
### Methods
| Name | IN | IN values | OUT | OUT values |
|-|:-:|-|:-:|-|
| Trash | as | <ul><li>Array of fullpaths to be moved to trash</li></ul> | as | Trashed files |
| Erase | as | <ul><li>Array of trashed files fullpaths to be unlinked from fs</li></ul> | as | Erased files |
| EraseAll | | | as | Erased files |
| Restore | as | <ul><li>Array of trashed files fullpaths to be restored</li></ul> | as | Restored positions for every file |
| RestoreAll | | | as | Restored position for every file |
| List | | | as | List of trashed files |
| Size | | | t | Current trash size in bytes |
| Length | | | u | Current number of trashed elements |

### Signals
| Name | When | OUT | OUT values |
|-|:-:|-|:-:|
| Trashed | A new file has been moved to trash | s | In-trash fullpath of trashed file |
| Erased | A file has been completely erased | s | In-trash fullpath of erased file |
| Restored | A file has been restored | s | In-trash fullpath of restored file |

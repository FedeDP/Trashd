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
| Trash | as | Array of fullpaths to be moved to trash | as | Trashed files |
| Erase | as | Array of trashed files fullpaths to be unlinked from fs | as | Erased files |
| EraseAll | | | as | Erased files |
| Restore | as | Array of trashed files fullpaths to be restored | as | Restored position for every file |
| RestoreAll | | | as | Restored position for every file |
| TrashDate | s | Trashed file fullpath | s | Trashing date for the file |
| List | | | as | List of all trashed files |
| Size | | | t | Current trash size in bytes |
| Length | | | u | Current number of trashed elements |

### Signals
| Name | When | OUT | OUT values |
|-|:-:|-|:-:|
| TrashChanged | A change happened in any watched trash | | |

#### Explanation
This signal is to let FMs/DEs update their list of trashed files in their UI.  
When this signal is received, FMs will have to call "List" method to update their list of trashed files.
The signal is sent whenever a file is trashed, erased, restored, or whenever a new trash topdir is discovered (ie: a new filesystem has been mounted).

## Topdirs support
FMs implementing trashd interface should show a list of trashed files from all the mounted filesystems, plus the local one (home-trash).  
Trashd supports this kind of trash, ie: all methods will return Size, Length and List of all files in every known (currently mounted) trash. 

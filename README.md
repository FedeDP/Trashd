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
| Restore | as | <ul><li>Array of trashed files fullpaths to be restored</li></ul> | as | Restored position for every file |
| RestoreAll | | | as | Restored position for every file |
| List | s | <ul><li>Device to list files for (/dev/sdXY)</li></ul> | as | List of trashed files for specified device |
| ListAll | | | as | List of all trashed files |
| Size | | | t | Current trash size in bytes |
| Length | | | u | Current number of trashed elements |

### Signals
| Name | When | OUT | OUT values |
|-|:-:|-|:-:|
| Trashed | A new file has been moved to trash | s | In-trash fullpath of trashed file |
| Erased | A file has been completely erased | s | In-trash fullpath of erased file |
| Restored | A file has been restored | s | In-trash fullpath of restored file |
| TrashAdded | A new trash has been added (after a device has been mounted) | s | Devpath of the new device (to be passed to List method) |
| TrashRemoved | A trash has been removed (after a device has been unmounted) | s | Devpath of the device |

#### Explanation
These signals are sent to let FMs/DEs update their list of trashed files in their UI without needing to call "ListAll" method again.  
When a Trashed/Erased/Restored signal is emitted, FMs can be sure that the file has really been trashed/erased/restored, thus they can update their list removing only that file.  
TrashAdded/TrashRemoved signals are needed so that FMs know that their list of files has to be updated.  
In case of TrashAdded, they can just call "List" method passing received devpath and add these files to their list.  
In case of TrashRemoved, a ListAll call is the simplest way to update trashed files list.  

I can probably avoid all these signals and just send a "TrashChanged" signal, and let FMs call "ListAll" everytime. I'm still thinking about the better method.  
Feedbacks by developers are much appreciated about this point, see: https://github.com/FedeDP/Trashd/issues/1.

## Topdirs support
FMs implementing trashd interface should show a list of trashed files from all the mounted filesystems, plus the local one (home-trash).  
Trashd supports this kind of trash, ie: all methods will return Size, Length and List of all files in every known (currently mounted) trash. 

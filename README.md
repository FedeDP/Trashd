# Trashd

[![Build Status](https://travis-ci.org/FedeDP/Trashd.svg?branch=master)](https://travis-ci.org/FedeDP/Trashd)
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/032c66c7f6624e4e954168355c1648fa)](https://www.codacy.com/app/FedeDP/Trashd?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=FedeDP/Trashd&amp;utm_campaign=Badge_Grade)

Linux bus user service that provides an implementation of freedesktop trash spec: https://specifications.freedesktop.org/trash-spec/trashspec-latest.html.  
It aims to be used by file managers/DE as a DE agnostic, generic solution to manage trash.  

## Build
It only depends on libsystemd (systemd/sd-bus.h) and libudev (libudev.h).  

    $ make
    # make install
    
## Runtime dep
Top directories trashing support requires UDisks2 available at runtime. This is needed to react to newly mounted filesystems.  
If UDisks2 is not available, topdir support will not work, ie: Trashd will only support home trash.  

## Availability
Trashd is available for Archlinux users in AUR: https://aur.archlinux.org/packages/trashd-git/.  

## Interface
### Methods
| Name | IN | IN values | OUT | OUT values |
|-|:-:|-|:-:|-|
| Trash | as | Array of fullpaths to be moved to trash | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| Erase | as | Array of trashed files fullpaths to be unlinked from fs | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| EraseAll | | | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| Restore | as | Array of trashed files fullpaths to be restored | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| RestoreAll | | | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| TrashDate | as | List of trashed file fullpaths | a(sbs) | Array of [OUT structs](https://github.com/FedeDP/Trashd#out-struct) |
| List | | | as | List of all trashed files |
| Size | | | t | Current trash size in bytes |
| Length | | | u | Current number of trashed elements |

#### OUT struct
Trash, Erase{All}, Restore{All} and TrashDate methods will send back an array of struct of type "(sbs)".  
This struct is:

    { 
        .path = string: "path received as input", 
        .ok = boolean: false if error happened, 
        .output = string: ok ? "output value" : "error string" 
    }

This way, for each input value, developer can know if any error happened by just parsing ok boolean value, and they can get a proper error string too.  
Remember that Erase{All} methods will have output value as NULL, as it carries no informations if no error happens.  
Path will be set in both case to any file erased path. In case of error, output will have string error value.  

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

## Performance
The (very simple) [test.sh](https://github.com/FedeDP/Trashd/blob/master/test.sh) script tries to understand Trashd performance.  
It obviously computes performance on main user trash folder, that will surely be most used one.  
Its results, on my pc with SSD, are the following:  

    ./test.sh 
    Files creation time: 274 ms
    Trashing time: 49 ms
    Number of trashed files: u 500
    Listing time: 6 ms
    Erasing time: 18 ms
    Number of trashed files: u 0


Test does create 500 files with touch, then trashes them, lists them and finally erases them. Obviously these files are empty.  
As you can see most of the time is spent creating files. Beware that with many more files (eg: 5000 or 10000) it seems bus connection hangs and then times out, even if files are properly trashed/erased/listed.  
This is a weird issue given that Trashd is not hanging anywhere; it seems like a busctl limit.  
In fact, when adding "--expect-reply=false" flag to busctl, operations are instantaneous.

## Sample code
In [Sample](https://github.com/FedeDP/Trashd/blob/master/Sample) folder, you can find a small C sample using sd-bus library.

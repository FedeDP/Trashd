## Trashd Sample

This is a small sample application for Trashd.  
It shows how to use trashd through sd-bus library.  
It adds a match on "TrashChanged" signal, then shows how files can be trashed and restored,  
and how these operations emit a "TrashChanged" signal.  

It only needs libsystemd (sd-bus.h).  
To build, run:

     make sample
   
inside Trashd root folder.

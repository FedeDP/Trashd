## TODO

- [ ] Make it deb available
- [x] Add it to aur
- [x] add it to travis
- [x] add a small test bash script
- [x] add a small c demo application
- [x] add COPYING file and gpl license header
- [x] FIX: RestoreAll should return as path correct trashed filepath, not trashed info path
- [x] FIX: remove_line_from_directorysizes in case user passes a string with final "/"
- [ ] always check for Trash folder to exist, and create it if needed. Not only during startup.
- [x] Restore should check when renaming too (eg: create foo.txt, trash it, create another foo.txt, trash it again. Now restoreAll will fail on second file as they got same pathname)
- [ ] Propose it as a standard implementation

## TODO

- [ ] Make it deb available
- [ ] Add it to aur
- [x] add it to travis
- [x] add a small test bash script
- [x] add a small c demo application
- [x] add COPYING file and gpl license header
- [x] FIX: RestoreAll should return as path correct trashed filepath, not trashed info path
- [x] FIX: remove_line_from_directorysizes in case user passes a string with final "/"
- [ ] use realpath when needed (eg: when trashing a file, trash its realpath) -> on every method that receives a path from user?
- [ ] Propose it as a standard implementation

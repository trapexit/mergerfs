# follow-symlinks

This feature, when enabled, will cause symlinks to be interpreted by
mergerfs as their target.

When there is a getattr/stat request for a file mergerfs will check if
the file is a symlink and depending on the `follow-symlinks` setting
will replace the information about the symlink with that of that which
it points to.

When unlink'ing or rmdir'ing the followed symlink it will remove the
symlink itself and not that which it points to.

* `follow-symlinks=never`: Behave as normal. Symlinks are treated as such.
* `follow-symlinks=directory`: Resolve symlinks only which point to directories.
* `follow-symlinks=regular`: Resolve symlinks only which point to regular files.
* `follow-symlinks=all`: Resolve all symlinks to that which they point
  to. Symlinks which do not point to anything are left as is.
* Defaults to `never`.

**WARNING:** This feature should be considered experimental. There
might be edge cases yet found. If you find any odd behaviors please
file a ticket on [github](https://github.com/trapexit/mergerfs/issues/new?assignees=&labels=bug%2C+investigating&projects=&template=bug_report.md&title=).

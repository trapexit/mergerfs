# nfsopenhack

* `nfsopenhack=off`: No hack applied.
* `nfsopenhack=git`: Apply hack if path includes `/.git/`.
* `nfsopenhack=all`: Apply hack on all empty read-only files opened
  for writing.
* Defaults to `off`.

NFS is not fully POSIX compliant and historically certain behaviors,
such as opening files with `O_EXCL`, are not or not well
supported. When mergerfs (or any FUSE filesystem) is exported over NFS
some of these issues come up due to how NFS and FUSE interact.

This hack addresses the issue where the creation of a file with a
read-only mode but with a read/write or write only flag. Normally this
is perfectly valid but NFS chops the one open call into multiple
calls. Exactly how it is translated depends on the configuration and
versions of the NFS server and clients but it results in a permission
error because a normal user is not allowed to open a read-only file as
writable.

Even though it's a more niche situation this hack breaks normal
security and behavior and as such is `off` by default. If set to `git`
it will only perform the hack when the path in question includes
`/.git/`.  `all` will result in it applying anytime a read-only file
which is empty is opened for writing.

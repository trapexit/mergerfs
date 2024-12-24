# inodecalc

Inodes (`st_ino`) are unique identifiers within a filesystem. Each
mounted filesystem has device ID (st_dev) as well and together they
can uniquely identify a file on the whole of the system. Entries on
the same device with the same inode are in fact references to the same
underlying file. It is a many to one relationship between names and an
inode. Directories, however, do not have multiple links on most
systems due to the complexity they add.

FUSE allows the server (mergerfs) to set inode values but not device
IDs. Creating an inode value is somewhat complex in mergerfs' case as
files aren't really in its control. If a policy changes what directory
or file is to be selected or something changes out of band it becomes
unclear what value should be used. Most software does not to care what
the values are but those that do often break if a value changes
unexpectedly. The tool find will abort a directory walk if it sees a
directory inode change. NFS can return stale handle errors if the
inode changes out of band. File dedup tools will usually leverage
device ids and inodes as a shortcut in searching for duplicate files
and would resort to full file comparisons should it find different
inode values.

mergerfs offers multiple ways to calculate the inode in hopes of
covering different usecases.

* `passthrough`: Passes through the underlying inode value. Mostly
  intended for testing as using this does not address any of the
  problems mentioned above and could confuse file deduplication
  software as inodes from different filesystems can be the same.
* `path-hash`: Hashes the relative path of the entry in question. The
  underlying file's values are completely ignored. This means the
  inode value will always be the same for that file path. This is
  useful when using NFS and you make changes out of band such as copy
  data between branches. This also means that entries that do point to
  the same file will not be recognizable via inodes. That does not
  mean hard links don't work. They will.
* `path-hash32`: 32bit version of path-hash.
* `devino-hash`: Hashes the device id and inode of the underlying
  entry. This won't prevent issues with NFS should the policy pick a
  different file or files move out of band but will present the same
  inode for underlying files that do too.
* `devino-hash32`: 32bit version of devino-hash.
* `hybrid-hash`: Performs path-hash on directories and devino-hash on
  other file types. Since directories can't have hard links the static
  value won't make a difference and the files will get values useful
  for finding duplicates. Probably the best to use if not using
  NFS. As such it is the default.
* `hybrid-hash32`: 32bit version of hybrid-hash.

32bit versions are provided as there is some software which does not
handle 64bit inodes well.

While there is a risk of hash collision in tests of a couple of
million entries there were zero collisions. Unlike a typical
filesystem FUSE filesystems can reuse inodes and not refer to the same
entry. The internal identifier used to reference a file in FUSE is
different from the inode value presented. The former is the nodeid and
is actually a tuple of 2 64bit values: nodeid and generation. This
tuple is not client facing. The inode that is presented to the client
is passed through the kernel uninterpreted.

From FUSE docs for `use_ino`:

> Honor the st_ino field in the functions getattr() and
> fill_dir(). This value is used to fill in the st_ino field
> in the stat(2), lstat(2), fstat(2) functions and the d_ino
> field in the readdir(2) function. The filesystem does not
> have to guarantee uniqueness, however some applications
> rely on this value being unique for the whole filesystem.
> Note that this does *not* affect the inode that libfuse
> and the kernel use internally (also called the "nodeid").

**NOTE:** As of version 2.35.0 the use_ino option has been
removed. mergerfs should always be managing inode values.

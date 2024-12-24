# Known Issues and Bugs

## mergerfs

### Supplemental user groups

Due to the overhead of
[getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs
utilizes a cache. This cache is opportunistic and per thread. Each
thread will query the supplemental groups for a user when that
particular thread needs to change credentials and will keep that data
for the lifetime of the thread. This means that if a user is added to
a group it may not be picked up without the restart of
mergerfs. In the future this may be improved to allow a periodic or
manual clearing of the cache.

While not a bug some users have found when using containers that
supplemental groups defined inside the container don't work as
expected. Since mergerfs lives outside the container it is querying
the host's group database. Effectively containers have their own user
and group definitions unless setup otherwise just as different systems
would.

Users should mount in the host group file into the containers or use a
standard shared user & groups technology like NIS or LDAP.


### directory mtime is not being updated

Remember that the default policy for `getattr` is `ff`. The
information for the first directory found will be returned. If it
wasn't the directory which had been updated then it will appear
outdated.

The reason this is the default is because any other policy would be
more expensive and for many applications it is unnecessary. To always
return the directory with the most recent mtime or a faked value based
on all found would require a scan of all filesystems.

If you always want the directory information from the one with the
most recent mtime then use the `newest` policy for `getattr`.


### 'mv /mnt/pool/foo /mnt/disk1/foo' removes 'foo'

This is not a bug.

Run in verbose mode to better understand what's happening:

```
$ mv -v /mnt/pool/foo /mnt/disk1/foo
copied '/mnt/pool/foo' -> '/mnt/disk1/foo'
removed '/mnt/pool/foo'
$ ls /mnt/pool/foo
ls: cannot access '/mnt/pool/foo': No such file or directory
```

`mv`, when working across devices, is copying the source to target and
then removing the source. Since the source **is** the target in this
case, depending on the unlink policy, it will remove the just copied
file and other files across the branches.

If you want to move files to one filesystem just copy them there and
use mergerfs.dedup to clean up the old paths or manually remove them
from the branches directly.


### cached memory appears greater than it should be

Use `cache.files=off` and/or `dropcacheonclose=true`. See the section
on [page caching](config/cache.md).


### NFS clients returning ESTALE / Stale file handle

NFS generally does not like out of band changes. Take a look at the
section on NFS in the [remote-filesystems](remote_filesystems.md) for
more details.


### rtorrent fails with ENODEV (No such device)

Be sure to set
[cache.files=partial|full|auto-full|per-process](config/cache.md)
or use Linux kernel v6.6 or above. rtorrent and some other
applications use [mmap](http://linux.die.net/man/2/mmap) to read and
write to files and offer no fallback to traditional methods.


### Plex / Jellyfin doesn't work with mergerfs

 It does. If you're trying to put the software's config / metadata /
database on mergerfs you can't set
[cache.files=off](config/cache.md) (unless you use Linux v6.6 or
above) because Plex is using **sqlite3** with **mmap** enabled.

That said it is recommended that config and runtime files be stored on
SSDs on a regular filesystem for performance reasons and if you are
using HDDs in your pool to help limit spinup.

Other software that leverages **sqlite3** which require **mmap**
includes Radarr, Sonarr, Lidarr.

It is recommended that you reach out to the developers of the software
you're having troubles with and asking them to add a fallback to
regular file IO when **mmap** is unavailable. It is not only more
compatible and resilient but also can be more performant in certain
situations.

If the issue is that quick scanning doesn't seem to pick up media then
be sure to set `func.getattr=newest`, though generally, a full scan
will pick up all media anyway.


### When a program tries to move or rename a file it fails

Please read the docs regarding [rename and
link](config/rename_and_link.md).

The problem is that many applications do not properly handle `EXDEV`
errors which `rename` and `link` may return even though they are
perfectly valid situations which do not indicate actual device,
filesystem, or OS errors. The error will only be returned by mergerfs
if using a path preserving policy as described in the policy section
above. If you do not care about path preservation simply change the
mergerfs policy to the non-path preserving version. For example: `-o
category.create=mfs` Ideally the offending software would be fixed and
it is recommended that if you run into this problem you contact the
software's author and request proper handling of `EXDEV` errors.


### my 32bit software has problems

Some software have problems with 64bit inode values. The symptoms can
include EOVERFLOW errors when trying to list files. You can address
this by setting `inodecalc` to one of the 32bit based algos as
described in the relevant section.

### Moving files and directories fails with Samba

Workaround: Copy the file/directory and then remove the original
rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3
and prior (found in Ubuntu 14.04 among others) failed to handle
certain error codes correctly. Particularly `STATUS_NOT_SAME_DEVICE`
which comes from the `EXDEV` that is returned by `rename` when the
call is crossing mount points. When a program gets an `EXDEV` it needs
to explicitly take an alternate action to accomplish its goal. In the
case of `mv` or similar it tries `rename` and on `EXDEV` falls back to
a copying the file to the destination and deleting the source. In
these older versions of GVFS-fuse if it received `EXDEV` it would
translate that into `EIO`. This would cause `mv` or most any
application attempting to move files around on that SMB share to fail
with a generic IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568)
and above fixed this issue but a large number of systems use the older
release. On Ubuntu, the version can be checked by issuing `apt-cache
showpkg gvfs-fuse`. Most distros released in 2015 seem to have the
updated release and will work fine but older systems may
not. Upgrading gvfs-fuse or the distro in general will address the
problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with
their own product. It appears their new client does not handle
`EXDEV` either and responds similarly to older releases of gvfs on
Linux.


### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our
case that will really only happen with path preserving policies like
`epmfs`) and the software doesn't handle the situation well. This is
unfortunately a common failure of software which moves files
around. The standard indicates that an implementation **MAY** choose
to support non-user home directory trashing of files (which is a
**MUST**). The implementation **MAY** also support "top directory
trashes" which many probably do.

To create a `$topdir/.Trash` directory as defined in the standard use
the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool
`mergerfs.mktrash`.


## FUSE and Linux kernel

There have been a number of kernel issues / bugs over the years which
mergerfs has run into. Here is a list of them for reference and
posterity.


### NFS and EIO errors

[https://lore.kernel.org/linux-fsdevel/20240228160213.1988854-1-mszeredi@redhat.com/T/](https://lore.kernel.org/linux-fsdevel/20240228160213.1988854-1-mszeredi@redhat.com/T/)

Over the years some users have reported that while exporting mergerfs
via NFS, after significant filesystem activity, not only will the NFS
client start returning ESTALE and EIO errors but mergerfs itself would
start returning EIO errors. The problem was that no one could
reliability reproduce the issue. After a string of reports in late
2023 and early 2024 more investigation was done.

In Linux 5.14 new validation was put into FUSE which caught a few
invalid situations and would tag a FUSE node as invalid if a check
failed. Such checks include invalid file type, changing of type from
one request to another, a size greater than 63bit, and the generation
of a inode changing while in use.

What happened was that mergerfs was using a different fixed, non-zero
value for the generation of all nodes as it was suggested that unique
inode + generation pairs are needed for proper integration with
NFS. That non-zero value was being sent back to the kernel when a
lookup request was made for root. The reason this was hard to track
down was because NFS almost uniquely uses an API which can lead to a
lookup of the root node that simply won't happen under normal
workloads and usage. And that lookup will only happen if child nodes
of the root were forgotten but NFS still had a handle to that node and
later asked for details about it. It would trigger a set of requests
to lookup info on those nodes.

This wasn't a bug in FUSE but mergerfs. However, the incorrect
behavior of mergerfs lead to FUSE behave in an unexpected and
incorrect manner. It would issue a lookup of the "parent of a child of
the root" and mergerfs would send the invalid generation value. As a
result the kernel would mark the root node as "bad" which would then
trigger the kernel to issue a "forget root" message. In between those
it would issue a request for the parent of the root... which doesn't
exist.

So the kernel was doing two invalid things. Requesting the parent of
the root and then when that failed issuing a forget for the
root. These led to chasing after the wrong possible causes.

The change was for FUSE to revert the marking of root node bad if the
generation is non-zero and warn about it. It will mark the node bad
but not unhash/forget/remove it.

mergerfs in v2.40.1 ensures that generation for root is always 0 on
lookup which should work across any kernel version.


### Truncated files

This was a bug with `mmap` and `FUSE` on 32bit platforms. Should be fixed in all LTS releases.

* [https://marc.info/?l=linux-fsdevel&m=155550785230874&w=2](https://marc.info/?l=linux-fsdevel&m=155550785230874&w=2)


### Crashing on OpenVZ

There was a bug in the OpenVZ kernel with regard to how it handles `ioctl` calls. It was making invalid requests which would lead to crashes due to mergerfs not expecting them.

* [https://bugs.openvz.org/browse/OVZ-7145](https://bugs.openvz.org/browse/OVZ-7145)
* [https://www.mail-archive.com/devel@openvz.org/msg37096.html](https://www.mail-archive.com/devel@openvz.org/msg37096.html)


### Really bad mmap performance

There was a bug in caching which affects overall performance of `mmap` through `FUSE` in Linux 4.x kernels. It is fixed in 4.4.10 and 4.5.4.

* [https://lkml.org/lkml/2016/3/16/260](https://lkml.org/lkml/2016/3/16/260)
* [https://lkml.org/lkml/2016/5/11/59](https://lkml.org/lkml/2016/5/11/59)


### Heavy load and memory pressure leads to kernel panic

* [https://lkml.org/lkml/2016/9/14/527](https://lkml.org/lkml/2016/9/14/527)
* [https://lkml.org/lkml/2016/10/4/1](https://lkml.org/lkml/2016/10/4/1)
* [https://www.theregister.com/2016/10/05/linus_torvalds_admits_buggy_crap_made_it_into_linux_48/](https://www.theregister.com/2016/10/05/linus_torvalds_admits_buggy_crap_made_it_into_linux_48/)

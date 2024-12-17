# KNOWN ISSUES / BUGS

#### kernel issues & bugs

[https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs](https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs)

#### directory mtime is not being updated

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

#### 'mv /mnt/pool/foo /mnt/disk1/foo' removes 'foo'

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

#### cached memory appears greater than it should be

Use `cache.files=off` and/or `dropcacheonclose=true`. See the section
on page caching.

#### NFS clients returning ESTALE / Stale file handle

NFS generally does not like out of band changes. Take a look at the
section on NFS in the [remote-filesystems](remote_filesystems.md) for
more details.

#### rtorrent fails with ENODEV (No such device)

Be sure to set
`cache.files=partial|full|auto-full|per-processe`. rtorrent and some
other applications use [mmap](http://linux.die.net/man/2/mmap) to read
and write to files and offer no fallback to traditional methods. FUSE
does not currently support mmap while using `direct_io`. There may be
a performance penalty on writes with `direct_io` off as well as the
problem of double caching but it's the only way to get such
applications to work. If the performance loss is too high for other
apps you can mount mergerfs twice. Once with `direct_io` enabled and
one without it. Be sure to set `dropcacheonclose=true` if not using
`direct_io`.

#### Plex doesn't work with mergerfs

It does. If you're trying to put Plex's config / metadata / database
on mergerfs you can't set `cache.files=off` because Plex is using
sqlite3 with mmap enabled. Shared mmap is not supported by Linux's
FUSE implementation when page caching is disabled. To fix this place
the data elsewhere (preferable) or enable `cache.files` (with
`dropcacheonclose=true`). Sqlite3 does not need mmap but the developer
needs to fall back to standard IO if mmap fails.

This applies to other software: Radarr, Sonarr, Lidarr, Jellyfin, etc.

I would recommend reaching out to the developers of the software
you're having troubles with and asking them to add a fallback to
regular file IO when mmap is unavailable.

If the issue is that scanning doesn't seem to pick up media then be
sure to set `func.getattr=newest`, though generally, a full scan will
pick up all media anyway.

#### When a program tries to move or rename a file it fails

Please read the section above regarding [rename and link](functions_categories_and_policies.md#rename-and-link).

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

#### my 32bit software has problems

Some software have problems with 64bit inode values. The symptoms can
include EOVERFLOW errors when trying to list files. You can address
this by setting `inodecalc` to one of the 32bit based algos as
described in the relevant section.

#### Samba: Moving files / directories fails

Workaround: Copy the file/directory and then remove the original
rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3
and prior (found in Ubuntu 14.04 among others) failed to handle
certain error codes correctly. Particularly **STATUS_NOT_SAME_DEVICE**
which comes from the **EXDEV** which is returned by **rename** when
the call is crossing mount points. When a program gets an **EXDEV** it
needs to explicitly take an alternate action to accomplish its
goal. In the case of **mv** or similar it tries **rename** and on
**EXDEV** falls back to a manual copying of data between the two
locations and unlinking the source. In these older versions of
GVFS-fuse if it received **EXDEV** it would translate that into
**EIO**. This would cause **mv** or most any application attempting to
move files around on that SMB share to fail with a IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568)
and above fixed this issue but a large number of systems use the older
release. On Ubuntu, the version can be checked by issuing `apt-cache
showpkg gvfs-fuse`. Most distros released in 2015 seem to have the
updated release and will work fine but older systems may
not. Upgrading gvfs-fuse or the distro in general will address the
problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with
their own product. It appears their new client does not handle
**EXDEV** either and responds similarly to older releases of gvfs on
Linux.

#### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our
case that will really only happen with path preserving policies like
`epmfs`) and the software doesn't handle the situation well. This is
unfortunately a common failure of software which moves files
around. The standard indicates that an implementation `MAY` choose to
support non-user home directory trashing of files (which is a
`MUST`). The implementation `MAY` also support "top directory trashes"
which many probably do.

To create a `$topdir/.Trash` directory as defined in the standard use
the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool
`mergerfs.mktrash`.

#### Supplemental user groups

Due to the overhead of
[getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs
utilizes a cache. This cache is opportunistic and per thread. Each
thread will query the supplemental groups for a user when that
particular thread needs to change credentials and will keep that data
for the lifetime of the thread. This means that if a user is added to
a group it may not be picked up without the restart of
mergerfs. However, since the high level FUSE API's (at least the
standard version) thread pool dynamically grows and shrinks it's
possible that over time a thread will be killed and later a new thread
with no cache will start and query the new data.

The gid cache uses fixed storage to simplify the design and be
compatible with older systems which may not have C++11
compilers. There is enough storage for 256 users' supplemental
groups. Each user is allowed up to 32 supplemental groups. Linux >=
2.6.3 allows up to 65535 groups per user but most other \*nixs allow
far less. NFS allows only 16. The system does handle overflow
gracefully. If the user has more than 32 supplemental groups only the
first 32 will be used. If more than 256 users are using the system
when an uncached user is found it will evict an existing user's cache
at random. So long as there aren't more than 256 active users this
should be fine. If either value is too low for your needs you will
have to modify `gidcache.hpp` to increase the values. Note that doing
so will increase the memory needed by each thread.

While not a bug some users have found when using containers that
supplemental groups defined inside the container don't work properly
with regard to permissions. This is expected as mergerfs lives outside
the container and therefore is querying the host's group
database. There might be a hack to work around this (make mergerfs
read the /etc/group file in the container) but it is not yet
implemented and would be limited to Linux and the /etc/group
DB. Preferably users would mount in the host group file into the
containers or use a standard shared user & groups technology like NIS
or LDAP.

# Technical Behavior and Limitations

## Do hard links work?

Yes. [links](https://en.wikipedia.org/wiki/Hard_link) (otherwise known
as hard links) are fundamentally supported by mergerfs. There is no
option to enable or disable links. All comments elsewhere claiming they
do not work are related to the setup of the user's system.

See the [Intro to Filesystems](../intro_to_filesystems.md) page for more
details on what a `link` is.

A limitation of a `link` is that it **must** reference a file/inode
within the same filesystem or device. When a request is made to the
operating system to `link` (or `rename`) a file to a target location
the OS will first check if the source and target are the same
filesystem / device. If not it will return the error `EXDEV` meaning
that the request is not able to be made due to the result being "cross
device".

What catches many users off guard is that this "cross device" check is
not very deep or involved. Even the same filesystem bind mounted to
another location will typically be considered a different device. So
when mergerfs is creating a union of other filesystems there is no way
to link or rename from one of the underlying branches into
mergerfs. They are different filesystems even if logically they seem
related or the same. And similarly it is impossible for mergerfs to
truly make a link between two branches which are not in fact the same
mounted filesystem.

This is relevant to mergerfs users because bind mounts are very common
when using container runtimes such as Docker, Podman, or
Kubernetes. When you add a volume or mount to your container that is
in fact a bind mount of that path. Meaning if you were to add
`/mnt/mergerfs/downloads` and `/mnt/mergerfs/movies` as separate
mounts into your container any request to link between the two
locations within the container will fail with the `EXDEV` error even
though it will work perfectly fine on the host. If you wish for `link`
and `rename` to work you **MUST** mount into the container a point in
the directory structure above what you wish to use. In this case
`/mnt/mergerfs`.

Another situation where people will hit unexpected `EXDEV` errors is
when they use a policy which makes file creation decisions based on
existing paths. If the source of a `link` or `rename` is on branch A
but the target directory is on branch B, given the whole point of the
policy is to strictly enforce behavior based on what branches already
exist, it must return `EXDEV`. Doing otherwise would fundamentally
break the purpose of the policy. Read [rename &
link](../config/rename_and_link.md) for more details on how mergerfs
handles those two functions.

When in doubt it is trivial to test `link`:

```shell
$ cd /mnt/mergerfs/
$ touch foo
$ ln -v foo bar
'bar' => 'foo'
$ stat foo bar | grep Inode
Device: 0,173   Inode: 123456  Links: 2
Device: 0,173   Inode: 123456  Links: 2
```

For the more technical: within mergerfs the inode of a file is not
necessarily indicative of two file names linking to the same
underlying data. For more details as to why that is see the docs on
[inodecalc](../config/inodecalc.md).


## Are there any size limitations to files, directories, etc.?

Whatever the limits are of the underlying filesystems.


## Do reflink, FICLONE, or FICLONERANGE work?

Unfortunately not. FUSE, the technology mergerfs is based on, does not
support the `clone_file_range` feature needed for it to work. mergerfs
won't even know such a request is made. The kernel will simply return
an error back to the application making the request.

Should FUSE gain the ability mergerfs will be updated to support it.

That said: mergerfs itself will attempt to use `FICLONE` when copying
data between files in the limited cases where it copies/moves files.


## How does mergerfs handle moving and copying of files?

This is a _very_ common mistaken assumption regarding how filesystems
work. There is no such thing as "move" or "copy." These concepts are
high level behaviors made up of numerous independent steps and _not_
individual filesystem functions.

A "move" can include a "copy" so lets describe copy first.

When an application copies a file from "source" to "destination" it
can do so in a number of ways but the basics are the following.

1. `open` the source file.
2. `create` the destination file.
3. `read` a chunk of data from source and `write` to
   destination. Continue till it runs out of data to copy.
4. Copy file metadata (`stat`) such as ownership (`chown`),
   permissions (`chmod`), timestamps (`utimes`), extended attributes
   (`getxattr`, `setxattr`), etc.
5. `close` source and destination files.

"move" is typically a `rename(src,dst)` and if that errors with
`EXDEV` (meaning the source and destination are on different
filesystems) the application will "copy" the file as described above
and then it removes (`unlink`) the source.

The `rename(src,dst)`, `open(src)`, `create(dst)`, data copying,
metadata copying, `unlink(src)`, etc. are entirely distinct and
separate events. There is really no practical way to know that what is
ultimately occurring is the "copying" of a file or what the source
file would be. Since the source is not known there is no way to know
how large a created file is destined to become. This is why it is
impossible for mergerfs to choose the branch for a `create` based on
file size. The only context provided when a file is created, besides
the name, is the permissions, if it is to be read and/or written, and
some low level settings for the operating system.

All of this means that mergerfs can not make decisions when a file is
created based on file size or the source of the data. That information
is simply not available. At best mergerfs could respond to files
reaching a certain size when writing data, a file is closed, or
renamed.

Related: if a user wished to have mergerfs perform certain activities
based on the name of a file it is common and even best practice for a
program to write to a temporary file first and then rename to its
final destination. That temporary file name will typically be random
and have no indication of the type of file being written. At best
something could be done on rename.

Additional reading: [Intro to Filesystems:
Workflows](../intro_to_filesystems.md#workflows)


## Why does the total available space in mergerfs not equal outside?

Are you using ext2/3/4? With reserve for root? mergerfs uses available
space for statfs calculations. If you've reserved space for root then
it won't show up.

You can remove the reserve by running: `tune2fs -m 0 <device>`


## I notice massive slowdowns of writes when enabling cache.files.

When file caching is enabled in any form (`cache.files!=off`) it may
issue `getxattr` requests for `security.capability` prior to _every
single write_. This will usually result in performance degradation,
especially when using a network filesystem (such as NFS or SMB.)
Unfortunately at this moment, the kernel is not caching the response.

To work around this situation mergerfs offers a few solutions.

1. Set `security_capability=false`. It will short circuit any call and
   return `ENOATTR`. This still means though that mergerfs will
   receive the request before every write but at least it doesn't get
   passed through to the underlying filesystem.
2. Set `xattr=noattr`. Same as above but applies to _all_ calls to
   getxattr. Not just `security.capability`. This will not be cached
   by the kernel either but mergerfs' runtime config system will still
   function.
3. Set `xattr=nosys`. Results in mergerfs returning `ENOSYS` which
   _will_ be cached by the kernel. No future xattr calls will be
   forwarded to mergerfs. The downside is that also means the xattr
   based config and query functionality won't work either.
4. Disable file caching. If you aren't using applications which use
   `mmap` it's probably simpler to just disable it altogether. The
   kernel won't send the requests when caching is disabled.


## Why use FUSE? Why not a kernel based solution?

As with any solution to a problem, there are advantages and
disadvantages to each one.

A FUSE based solution has all the downsides of FUSE:

* Higher IO latency due to the trips in and out of kernel space
  (though now minimized with passthrough IO)
* Higher general overhead due to trips in and out of kernel space
* Double caching when using page caching
* Misc limitations due to FUSE's design

But FUSE also has a lot of upsides:

* Easier to offer a cross platform solution
* Easier forward and backward compatibility
* Easier updates for users
* Easier and faster release cadence
* Allows more flexibility in design and features
* Overall easier to write, secure, and maintain
* Much lower barrier to entry (getting code into the kernel takes a
  lot of time and effort initially)


## Is my OS's libfuse needed for mergerfs to work?

No. Normally `mount.fuse` is needed to get mergerfs (or any FUSE
filesystem to mount using the `mount` command but in vendoring the
libfuse library the `mount.fuse` app has been renamed to
`mount.mergerfs` meaning the filesystem type in `fstab` can simply be
`mergerfs`. That said there should be no harm in having it installed
and continuing to using `fuse.mergerfs` as the type in `/etc/fstab`.

If `mergerfs` doesn't work as a type it could be due to how the
`mount.mergerfs` tool was installed. Must be in `/sbin/` with proper
permissions.


## Does mergerfs use splice?

On Linux, for large reads only, automatically. When a `read` request
is 64KiB or larger, mergerfs splices the data from the branch file
through a pipe into `/dev/fuse` instead of reading it into its own
memory and writing it back out. The data never crosses userspace, which
saves one copy and some CPU per request. Smaller reads, all writes and
every other request use the normal copy path, because splicing them
costs more in extra syscalls than it saves: earlier versions that
spliced every request were measurably slower for small I/O and metadata
operations.

There is nothing to configure. If splicing a reply is not possible the
read is served through the copy path instead: a branch filesystem that
does not support splice is detected on the first attempt and skipped for
that open file, and a read extending past the size the branch reports
for the file (the last part of a file, files smaller than the read,
files reporting a size of 0 such as procfs files, and files reporting a
nominal 4096 bytes such as sysfs attributes) uses the copy path, so a
branch whose reported size lags its data still
returns everything. Files on FUSE, SMB/CIFS, NFS, Ceph, AFS, overlayfs
or 9p branches, and any file opened `O_DIRECT`, always use the copy
path: those filesystems' splice_read can serve a page cache that their
normal reads bypass or first revalidate, so a spliced reply could
return stale data, and for an `O_DIRECT` file the kernel would serve
the splice by direct I/O into newly allocated pages, which saves
nothing over the copy path.
A read that arrives while mergerfs is moving the file to another
branch (`moveonenospc`) also uses the copy path.
Unless mergerfs runs with
`CAP_SYS_RESOURCE` (as root on a host normally does, but root in a
container usually does not unless started with `--cap-add=SYS_RESOURCE`
or `--privileged`; `CAP_SYS_ADMIN` is not enough), the `pipe-max-size`
sysctl limits the pipe a reply can use. A reply needs one page more
than the read (for the reply header), two when the read does not start
on a page boundary, and the kernel rounds that up to a power of two, so
`pipe-max-size` must be larger than `fuse-msg-size` for the largest
reads to be spliced: with the defaults (both 1MiB) 1MiB reads use the
copy path while smaller reads keep splicing, with a single log
message. That limit is read when mergerfs starts, so raising
`pipe-max-size` takes effect after mergerfs is restarted. Only a kernel
without splice support, or a seccomp policy that refuses it, disables it
for the whole process. The old
`splice_read`, `splice_write` and `splice_move` options are still
accepted and do nothing.

Two behaviors of spliced reads differ from the copy path. The data is
copied into the reply only after the branch filesystem's read has
returned, so a write or truncate of the branch file racing a read can
produce a reply mixing old and new contents, even on filesystems such as
XFS that otherwise serialize reads against writes. And on kernels before
6.4, spliced reads do not generate inotify or fanotify access events
(`IN_ACCESS`, `FAN_ACCESS`) on the branch file.

Files using [passthrough.io](../config/passthrough.md) never reach
mergerfs for reads, so splice does not apply to them, unless
`direct-io` is also in effect for them, which overrides passthrough.


## How does mergerfs handle credentials?

mergerfs is a multithreaded application in order to handle requests
from the kernel concurrently. Each FUSE message has a header with
certain details about the request including the process ID (pid) of
the requesting application, the process' effective user id (uid), and
group id (gid).

FUSE and the kernel have two ways of managing permissions. A kernel
side `default_permissions` option and leaving it to the FUSE
server. When default permissions is enabled the kernel will do the
entitlement checks and only allow requests through which should be
allowed according to normal POSIX permissions. When not enabled it is
the responsibility of the FUSE server, in this case mergerfs, to do
whatever is necessary to manage entitlements.

Prior to mergerfs v2.42.0 it would enable `default_permissions` but
also leveraged the uid and gid available in each FUSE request. The
thread actioning the request would change its credentials to match
those of the requesting application to ensure permissions were
properly handled as well as dealing with some quirks of non-POSIX
compatible filesystems. However, this strategy had two major
issues. First, it was not compatible with the new `allow-idmap`
feature of FUSE which allows a filesystem to advertise it can be used
with id mapping which is often used with containers. Secondly, it
caused permission issues when the kernel would say something was
allowed but due to the way mergerfs was changing creds would
fail. This mostly came in the form of `chroot`ed setups like used in
containers. Neither of these tended to impact casual users but did
break some niche or power user use cases. Since the casual user won't
notice the subtle changes and it would enable new use cases it was
decided that the credential handling would change.

As of v2.42.0 mergerfs now runs as root more generally. Either
changing credentials when creating files (Linux) or chown'ing them
after creation (FreeBSD).

As a result of this change it is now necessary for the FUSE
`default_permissions` feature be used for proper entitlements
management. mergerfs does allow it to be disabled but it should only
be done so for debugging purposes.


## Does mergerfs support idmap?

Yes, by setting [allow-idmap=true](../config/options.md) (which is the
default.)

Requires that
[kernel-permissions-check](../config/kernel-permissions-check.md) be
enabled (the default.)


## What happens if a branch filesystem blocks?

POSIX filesystem calls unfortunately are entirely synchronous. If the
underlying filesystem mergerfs is interacting with freezes up or
blocks then the thread issuing the request will block same as any
other piece of software. If enough threads block then mergerfs will
block. There are no timeouts or ways to truly work around this
situation. If mergerfs spawned new threads it is very likely they too
would end up blocked.


## What information does mergerfs have when making decisions?

Every incoming request contains:

* The `tid` or `pid` of the calling thread/process
* The `uid` and `gid` of the calling thread/process
* The `umask` of the calling thread/process
* The arguments of the filesystem call in question

Naturally the mergerfs config as well as anything queriable from the
operating system or filesystems are also available.


## Why does running `mount -a` result in new instances of mergerfs?

See [lazy-umount-mountpoint](../config/lazy-umount-mountpoint.md)

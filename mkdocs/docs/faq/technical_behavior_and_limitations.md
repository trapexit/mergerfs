# Technical Behavior and Limitations

## Do hardlinks work?

Yes. There is no option to enable or disable links (or renames.) They
are fundamentally supported in compatible situations. That said the
inode of a file is not necessarily indicative of two file names
linking to the same underlying data. See also the option `inodecalc`
for how inode values are calculated.

What mergerfs does not do is fake hard links across branches. Read the
section [rename & link](../config/rename_and_link.md) for how it
works.

Remember that hardlinks will **NOT** work across devices. That
includes between the original filesystem and a mergerfs pool, between
two separate pools of the same underlying filesystems, or bind mounts
of paths within the mergerfs pool. The latter is common when using
Docker or Podman. Multiple volumes (bind mounts) to the same
underlying filesystem are considered different devices. There is no
way to link or rename between them. You should mount in the highest
directory in the mergerfs pool that includes all the paths you need if
you want links and rename to work.


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


## Does FICLONE or FICLONERANGE work?

Unfortunately not. FUSE, the technology mergerfs is based on, does not
support the `clone_file_range` feature needed for it to work. mergerfs
won't even know such a request is made. The kernel will simply return
an error back to the application making the request.

Should FUSE gain the ability mergerfs will be updated to support it.


## Why does the total available space in mergerfs not equal outside?

Are you using ext2/3/4? With reserve for root? mergerfs uses available
space for statfs calculations. If you've reserved space for root then
it won't show up.

You can remove the reserve by running: `tune2fs -m 0 <device>`


## I notice massive slowdowns of writes when enabling cache.files.

When file caching is enabled in any form (`cache.files!=off`) it will
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

- Higher IO latency due to the trips in and out of kernel space
- Higher general overhead due to trips in and out of kernel space
- Double caching when using page caching
- Misc limitations due to FUSE's design

But FUSE also has a lot of upsides:

- Easier to offer a cross platform solution
- Easier forward and backward compatibility
- Easier updates for users
- Easier and faster release cadence
- Allows more flexibility in design and features
- Overall easier to write, secure, and maintain
- Much lower barrier to entry (getting code into the kernel takes a
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


## Why was splice support removed?

After a lot of testing over the years, splicing always appeared to
at best, provide equivalent performance, and in some cases, worse
performance. Splice is not supported on other platforms forcing a
traditional read/write fallback to be provided. The splice code was
removed to simplify the codebase.


## How does mergerfs handle credentials?


mergerfs is a multithreaded application in order to handle requests
from the kernel concurrently. Each FUSE message has a header with
certain details about the request include the process ID (pid) of the
requesting application, the process' effective user id (uid), and
group id (gid). To ensure proper POSIX filesystem behavior and
security mergerfs must change its identity to match that of the
requester when performing the core filesystem function on the
underlying filesystem. On most Unix/POSIX based system a process and
all its threads are under the same uid and gid. However, on Linux each
thread may have its own credentials. This allows mergerfs to be
multithreaded and for each thread to change to the credentials
(seteuid,setegid) as required by the incoming message it is
handling. However, on FreeBSD this is not possible at the moment
(though there has been
[discussions](https://wiki.freebsd.org/Per-Thread%20Credentials) and
as such must change the credentials of the whole application when
actioning messages. mergerfs does optimize this behavior by only
changing credentials and locking the thread to do so if the process is
currently not the same as what is necessary by the incoming request.

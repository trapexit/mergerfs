# CACHING

#### page caching

https://en.wikipedia.org/wiki/Page_cache

- cache.files=off: Disables page caching. Underlying files cached,
  mergerfs files are not.
- cache.files=partial: Enables page caching. Underlying files cached,
  mergerfs files cached while open.
- cache.files=full: Enables page caching. Underlying files cached,
  mergerfs files cached across opens.
- cache.files=auto-full: Enables page caching. Underlying files
  cached, mergerfs files cached across opens if mtime and size are
  unchanged since previous open.
- cache.files=libfuse: follow traditional libfuse `direct_io`,
  `kernel_cache`, and `auto_cache` arguments.
- cache.files=per-process: Enable page caching (equivalent to
  `cache.files=partial`) only for processes whose 'comm' name matches
  one of the values defined in `cache.files.process-names`. If the
  name does not match the file open is equivalent to
  `cache.files=off`.

FUSE, which mergerfs uses, offers a number of page caching modes. mergerfs tries to simplify their use via the `cache.files`
option. It can and should replace usage of `direct_io`,
`kernel_cache`, and `auto_cache`.

Due to mergerfs using FUSE and therefore being a userland process
proxying existing filesystems the kernel will double cache the content
being read and written through mergerfs. Once from the underlying
filesystem and once from mergerfs (it sees them as two separate
entities). Using `cache.files=off` will keep the double caching from
happening by disabling caching of mergerfs but this has the side
effect that _all_ read and write calls will be passed to mergerfs
which may be slower than enabling caching, you lose shared `mmap`
support which can affect apps such as rtorrent, and no read-ahead will
take place. The kernel will still cache the underlying filesystem data
but that only helps so much given mergerfs will still process all
requests.

If you do enable file page caching,
`cache.files=partial|full|auto-full`, you should also enable
`dropcacheonclose` which will cause mergerfs to instruct the kernel to
flush the underlying file's page cache when the file is closed. This
behavior is the same as the rsync fadvise / drop cache patch and Feh's
nocache project.

If most files are read once through and closed (like media) it is best
to enable `dropcacheonclose` regardless of caching mode in order to
minimize buffer bloat.

It is difficult to balance memory usage, cache bloat & duplication,
and performance. Ideally, mergerfs would be able to disable caching for
the files it reads/writes but allow page caching for itself. That
would limit the FUSE overhead. However, there isn't a good way to
achieve this. It would need to open all files with O_DIRECT which
places limitations on what the underlying filesystems would be
supported and complicates the code.

kernel documentation: https://www.kernel.org/doc/Documentation/filesystems/fuse-io.txt

#### entry & attribute caching

Given the relatively high cost of FUSE due to the kernel <-> userspace
round trips there are kernel side caches for file entries and
attributes. The entry cache limits the `lookup` calls to mergerfs
which ask if a file exists. The attribute cache limits the need to
make `getattr` calls to mergerfs which provide file attributes (mode,
size, type, etc.). As with the page cache these should not be used if
the underlying filesystems are being manipulated at the same time as
it could lead to odd behavior or data corruption. The options for
setting these are `cache.entry` and `cache.negative_entry` for the
entry cache and `cache.attr` for the attributes
cache. `cache.negative_entry` refers to the timeout for negative
responses to lookups (non-existent files).

#### writeback caching

When `cache.files` is enabled the default is for it to perform
writethrough caching. This behavior won't help improve performance as
each write still goes one for one through the filesystem. By enabling
the FUSE writeback cache small writes may be aggregated by the kernel
and then sent to mergerfs as one larger request. This can greatly
improve the throughput for apps which write to files
inefficiently. The amount the kernel can aggregate is limited by the
size of a FUSE message. Read the `fuse_msg_size` section for more
details.

There is a small side effect as a result of enabling writeback
caching. Underlying files won't ever be opened with O_APPEND or
O_WRONLY. The former because the kernel then manages append mode and
the latter because the kernel may request file data from mergerfs to
populate the write cache. The O_APPEND change means that if a file is
changed outside of mergerfs it could lead to corruption as the kernel
won't know the end of the file has changed. That said any time you use
caching you should keep from using the same file outside of mergerfs
at the same time.

Note that if an application is properly sizing writes then writeback
caching will have little or no effect. It will only help with writes
of sizes below the FUSE message size (128K on older kernels, 1M on
newer).

#### statfs caching

Of the syscalls used by mergerfs in policies the `statfs` / `statvfs`
call is perhaps the most expensive. It's used to find out the
available space of a filesystem and whether it is mounted
read-only. Depending on the setup and usage pattern these queries can
be relatively costly. When `cache.statfs` is enabled all calls to
`statfs` by a policy will be cached for the number of seconds its set
to.

Example: If the create policy is `mfs` and the timeout is 60 then for
that 60 seconds the same filesystem will be returned as the target for
creates because the available space won't be updated for that time.

#### symlink caching

As of version 4.20 Linux supports symlink caching. Significant
performance increases can be had in workloads which use a lot of
symlinks. Setting `cache.symlinks=true` will result in requesting
symlink caching from the kernel only if supported. As a result it's
safe to enable it on systems prior to 4.20. That said it is disabled
by default for now. You can see if caching is enabled by querying the
xattr `user.mergerfs.cache.symlinks` but given it must be requested at
startup you can not change it at runtime.

#### readdir caching

As of version 4.20 Linux supports readdir caching. This can have a
significant impact on directory traversal. Especially when combined
with entry (`cache.entry`) and attribute (`cache.attr`)
caching. Setting `cache.readdir=true` will result in requesting
readdir caching from the kernel on each `opendir`. If the kernel
doesn't support readdir caching setting the option to `true` has no
effect. This option is configurable at runtime via xattr
`user.mergerfs.cache.readdir`.

#### tiered caching

Some storage technologies support what some call "tiered" caching. The
placing of usually smaller, faster storage as a transparent cache to
larger, slower storage. NVMe, SSD, Optane in front of traditional HDDs
for instance.

mergerfs does not natively support any sort of tiered caching. Most
users have no use for such a feature and its inclusion would
complicate the code. However, there are a few situations where a cache
filesystem could help with a typical mergerfs setup.

1. Fast network, slow filesystems, many readers: You've a 10+Gbps network
   with many readers and your regular filesystems can't keep up.
2. Fast network, slow filesystems, small'ish bursty writes: You have a
   10+Gbps network and wish to transfer amounts of data less than your
   cache filesystem but wish to do so quickly.

With #1 it's arguable if you should be using mergerfs at all. RAID
would probably be the better solution. If you're going to use mergerfs
there are other tactics that may help: spreading the data across
filesystems (see the mergerfs.dup tool) and setting `func.open=rand`,
using `symlinkify`, or using dm-cache or a similar technology to add
tiered cache to the underlying device.

With #2 one could use dm-cache as well but there is another solution
which requires only mergerfs and a cronjob.

1. Create 2 mergerfs pools. One which includes just the slow devices
   and one which has both the fast devices (SSD,NVME,etc.) and slow
   devices.
2. The 'cache' pool should have the cache filesystems listed first.
3. The best `create` policies to use for the 'cache' pool would
   probably be `ff`, `epff`, `lfs`, or `eplfs`. The latter two under
   the assumption that the cache filesystem(s) are far smaller than the
   backing filesystems. If using path preserving policies remember that
   you'll need to manually create the core directories of those paths
   you wish to be cached. Be sure the permissions are in sync. Use
   `mergerfs.fsck` to check / correct them. You could also set the
   slow filesystems mode to `NC` though that'd mean if the cache
   filesystems fill you'd get "out of space" errors.
4. Enable `moveonenospc` and set `minfreespace` appropriately. To make
   sure there is enough room on the "slow" pool you might want to set
   `minfreespace` to at least as large as the size of the largest
   cache filesystem if not larger. This way in the worst case the
   whole of the cache filesystem(s) can be moved to the other drives.
5. Set your programs to use the cache pool.
6. Save one of the below scripts or create you're own.
7. Use `cron` (as root) to schedule the command at whatever frequency
   is appropriate for your workflow.

##### time based expiring

Move files from cache to backing pool based only on the last time the
file was accessed. Replace `-atime` with `-amin` if you want minutes
rather than days. May want to use the `fadvise` / `--drop-cache`
version of rsync or run rsync with the tool "nocache".

_NOTE:_ The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.time-based-mover](https://raw.githubusercontent.com/trapexit/mergerfs/refs/heads/latest-release/tools/mergerfs.time-based-mover)

##### percentage full expiring

Move the oldest file from the cache to the backing pool. Continue till
below percentage threshold.

_NOTE:_ The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.percent-full-mover](https://raw.githubusercontent.com/trapexit/mergerfs/refs/heads/latest-release/tools/mergerfs.percent-full-mover)

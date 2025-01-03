# caching

## cache.files

Controls how [page caching](https://en.wikipedia.org/wiki/Page_cache)
works for mergerfs itself. Not the underlying filesystems.

* `cache.files=off`: Disables page caching for mergerfs.
* `cache.files=partial`: Enables page caching. Files are cached
  while open.
* `cache.files=full`: Enables page caching. Files are cached across
  opens.
* `cache.files=auto-full`: Enables page caching. Files are cached
  across opens if mtime and size are unchanged since previous open.
* `cache.files=per-process`: Enable page caching (equivalent to
  `cache.files=partial`) only for processes whose 'comm' name matches
  one of the values defined in cache.files.process-names. If the name
  does not match the file open is equivalent to `cache.files=off`.


Generally, enabling the page cache actually *harms*
performance[^1]. In part because it can lead to buffer bloat due to
the kernel caching both the underlying filesystem's file content as
well as the file through mergerfs. However, if you want to confirm
performance differences it is recommended that you perform some
benchmark to confirm which option works best for your setup.

Why then would you want to enable page caching if it consumes ~2x the
RAM as normal and is on average slower? Because it is the only way to
support
[mmap](https://man7.org/linux/man-pages/man2/mmap.2.html). `mmap` is a
way for programs to treat a file as if it is a contiguous RAM buffer
which is regularly used by a number of programs such as those that
leverage **sqlite3**. Despite `mmap` not being supported by all
filesystems it is unfortunately common for software to not have an
option to use regular file IO instead of `mmap`.

The good thing is that in Linux v6.6[^2] and above FUSE can now
transparently enable page caching when mmap is requested. This means
it should be safe to set `cache.files=off`. However, on Linux v6.5 and
below you will need to configure `cache.files` as you need.


[^1]: This is not unique to mergerfs and affects all FUSE
    filesystems. It is something that the FUSE community hopes to
    investigate at some point but as of early 2025 there are a number
    of major reworking going on with FUSE which needs to be finished
    first.
[^2]: [https://kernelnewbies.org/Linux_6.6#FUSE](https://kernelnewbies.org/Linux_6.6#FUSE)


## cache.entry

* `cache.entry=UINT`: Sets the number of seconds to cache
  entry queries. Defaults to `1`.

The kernel must ask mergerfs about the existence of files. The entry
cache caches that those details which limits the number of requests
sent to mergerfs.

The risk of setting this value, as with most any cache, is related to
[out-of-band](https://en.wikipedia.org/wiki/Out-of-band) changes. If
the filesystems are changed outside mergerfs there is a risk of files
which have been removed continuing to show as available. It will fail
gracefully if a phantom file is actioned on in some way so there is
little risk in setting the value much higher. Especially if there are
no out-of-band changes.


## cache.negative_entry

* `cache.negative_entry=UINT`: Sets the number of seconds to cache
  negative entry queries. Defaults to `1`.
  
This is a cache for negative entry query responses. Such as when a
file which does not exist is referenced.

The risk of setting this value, as with most any cache, is related to
[out-of-band](https://en.wikipedia.org/wiki/Out-of-band) changes. If
the filesystems are changed outside mergerfs there is a risk of files
which have been added outside mergerfs not appearing correctly till
the cache entry times out if there had been a request for the same
name within mergerfs which didn't exist. This is mostly an
inconvenience.


## cache.attr

* `cache.attr=UINT`: Sets the number of seconds to cache file
  attributes. Defaults to `1`.
  
This is a cache for file attributes and metadata such as that which is
collected by the
[stat](https://man7.org/linux/man-pages/man2/stat.2.html) system call
which is used when you run commands such as `find` or `ls -lh`. 

As with other caches the risk of enabling the attribute cache is if
changes are made to the file out-of-band there could be
inconsistencies between the actual file and the cached details which
could result in different issues depending on how the data is used. If
the simultaneous writing of a file from inside and outside is unlikely
then you should be safe. That said any simultaneous, uncoordinated
manipulation of a file can lead to unexpected results.


## cache.statfs

* `cache.statfs=UINT`: Sets the number of seconds to cache `statfs`
  calls used by policies. Defaults to `0`.
  
A number of policies require looking up the available space of the
branches being considered. This is accomplished by calling
[statfs](https://man7.org/linux/man-pages/man2/statfs.2.html). This
call however is a bit expensive so this cache reduces the overhead by
limiting how often the calls are actually made.

This will mean that if the available space of branches changed
somewhat rapidly there is a risk of `create` or `mkdir` calls made
within the timeout period ending up on the same branch. This however
should even itself out over time.


## cache.symlinks

* `cache.symlinks=BOOL`: Enable kernel caching of symlink
  values. Defaults to `false`.
  
As of Linux v4.20 there is an ability to cache the value of symlinks
so that the kernel does not need to make a request to mergerfs every
single time a
[readlink](https://man7.org/linux/man-pages/man2/readlink.2.html)
request is made. While not a common usage pattern, if software very
regularly queries symlink values, the use of this cache could
significantly improve performance.

mergerfs will not error if the kernel used does not support symlink
caching.

As with other caches the main risk in enabling it is if you are
manipulating symlinks from both within and without the mergerfs
mount. Should the value be changed outside of mergerfs then it will
not be reflected in the mergerfs mount till the cached value is
invalidated.


## cache.readdir

* `cache.readdir=BOOL`: Enable kernel caching of readdir
  results. Defaults to `false`.
  
As of Linux v4.20 it supports readdir caching. This can have a
significant impact on directory traversal. Especially when combined
with entry (`cache.entry`) and attribute (`cache.attr`) caching. If
the kernel doesn't support readdir caching setting the option to true
has no effect. This option is configurable at runtime via xattr
user.mergerfs.cache.readdir.

## cache.writeback

* `cache.writeback=BOOL`: Enable writeback cache. Defaults to `false`.

When `cache.files` is enabled the default is for it to perform
writethrough caching. This behavior won't help improve performance as
each write still goes one for one through the filesystem. By enabling
the FUSE writeback cache small writes *may* be aggregated by the
kernel and then sent to mergerfs as one larger request. This can
greatly improve the throughput for apps which write to files
inefficiently. The amount the kernel can aggregate is limited by the
size of a FUSE message. Read the fuse_msg_size section for more
details.

There is a side effect as a result of enabling writeback
caching. Underlying files won't ever be opened with O_APPEND or
O_WRONLY. The former because the kernel then manages append mode and
the latter because the kernel may request file data from mergerfs to
populate the write cache. The O_APPEND change means that if a file is
changed outside of mergerfs it could lead to corruption as the kernel
won't know the end of the file has changed. That said any time you use
caching you should keep from writing to the same file outside of
mergerfs at the same time.

Note that if an application is properly sizing writes then writeback
caching will have little or no effect. It will only help with writes
of sizes below the FUSE message size (128K on older kernels, 1M on
newer). Even then its effectiveness might not be great. Given the side
effects of enabling this feature it is recommended that its benefits
be proved out with benchmarks.

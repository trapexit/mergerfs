# Tweaking Performance

**mergerfs** is effectively a filesystem proxy and therefore its
theoretical max performance is that of the underlying devices
(ignoring caching performed by the kernel.) However, given it is a
FUSE based filesystem working from userspace and it must combine the
behavior and information from multiple underlying branches there can
be an increase in overhead relative to other solutions. That said the
performance of certain functions, such as IO, can match the
theoretical max but it depends greatly on the system's
configuration. There are many things which can impact
performance. Device speeds and latency, network speeds and latency,
concurrency and parallel limits of the hardware, read/write sizes, the
number of branches, etc.

While some settings can impact performance they are all **functional**
in nature. Meaning they change mergerfs' behavior in some way. As a
result there is really no such thing as a "performance mode".

If you're having performance concerns please read over the
[benchmarking section](benchmarking.md) of these docs and then the
details below.

NOTE: Be sure to [read about available features](config/options.md)
before changing them to understand how functionality will change.

* test theoretical performance using `nullrw` or using a ram disk as a
  branch
* enable [passthrough.io](config/passthrough.md) (likely to have the
  biggest impact)
* change read or process [thread pools](config/threads.md)
* change [func.readdir](config/func_readdir.md)
* increase [readahead](config/readahead.md): `readahead=1024`
* disable `security-capability` and/or [xattr](config/xattr.md)
* increase cache timeouts [cache.attr](config/cache.md#cacheattr),
  [cache.entry](config/cache.md#cacheentry),
  [cache.negative-entry](config/cache.md#cachenegative-entry)
* toggle [page caching](config/cache.md#cachefiles)
* enable `parallel-direct-writes`
* enable [cache.statfs](config/cache.md#cachestatfs)
* enable [cache.symlinks](config/cache.md#cachesymlinks)
* enable [cache.readdir](config/cache.md#cachereaddir)
* disable `posix-acl`
* disable `async-read`
* use [symlinkify](config/symlinkify.md) if your data is largely
  static and read-only
* use [tiered cache](extended_usage_patterns.md) devices
* use LVM and LVM cache to place a SSD in front of your HDDs


## Additional Reading

* [Benchmarking](benchmarking.md)
* [Options](config/options.md)
* [Tips and Notes](tips_notes.md)

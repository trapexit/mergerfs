# Tweaking Performance

mergerfs is at its is a proxy and therefore its theoretical max
performance is that of the underlying devices. However, given it is a
FUSE based filesystem working from userspace there is an increase in
overhead relative to kernel based solutions. That said the performance
can match the theoretical max but it depends greatly on the system's
configuration. Especially when adding network filesystems into the mix
there are many variables which can impact performance. Device speeds
and latency, network speeds and latency, concurrency and parallel
limits of the hardware, read/write sizes, etc.

While some settings can impact performance they are all **functional**
in nature. Meaning they change mergerfs' behavior in some way. As a
result there is no such thing as a "performance mode".

If you're having performance issues please look over the suggestions
below and the [benchmarking section.](benchmarking.md)

NOTE: Be sure to [read about these features](config/options.md) before
changing them to understand how functionality will change.

* test theoretical performance using `nullrw` or mounting a ram disk
* increase readahead: `readahead=1024`
* disable `security_capability` and/or `xattr`
* increase cache timeouts `cache.attr`, `cache.entry`, `cache.negative_entry`
* enable (or disable) page caching (`cache.files`)
* enable `parallel-direct-writes`
* enable `cache.writeback`
* enable `cache.statfs`
* enable `cache.symlinks`
* enable `cache.readdir`
* change the number of threads available
* disable `posix_acl`
* disable `async_read`
* use `symlinkify` if your data is largely static and read-only
* use tiered cache devices
* use LVM and LVM cache to place a SSD in front of your HDDs

If you come across a setting that significantly impacts performance
please [contact trapexit](support.md) so he may investigate further. Please test
both against your normal setup, a singular branch, and with
`nullrw=true`

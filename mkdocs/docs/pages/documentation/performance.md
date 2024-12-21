# PERFORMANCE

mergerfs is at its core just a proxy and therefore its theoretical max
performance is that of the underlying devices. However, given it is a
FUSE filesystem working from userspace there is an increase in
overhead relative to kernel based solutions. That said the performance
can match the theoretical max but it depends greatly on the system's
configuration. Especially when adding network filesystems into the mix
there are many variables which can impact performance. Device speeds
and latency, network speeds and latency, general concurrency,
read/write sizes, etc. Unfortunately, given the number of variables it
has been difficult to find a single set of settings which provide
optimal performance. If you're having performance issues please look
over the suggestions below (including the benchmarking section.)

NOTE: be sure to read about these features before changing them to
understand what behaviors it may impact

- disable `security_capability` and/or `xattr`
- increase cache timeouts `cache.attr`, `cache.entry`, `cache.negative_entry`
- enable (or disable) page caching (`cache.files`)
- enable `parallel-direct-writes`
- enable `cache.writeback`
- enable `cache.statfs`
- enable `cache.symlinks`
- enable `cache.readdir`
- change the number of worker threads
- disable `posix_acl`
- disable `async_read`
- test theoretical performance using `nullrw` or mounting a ram disk
- use `symlinkify` if your data is largely static and read-only
- use tiered cache devices
- use LVM and LVM cache to place a SSD in front of your HDDs
- increase readahead: `readahead=1024`

If you come across a setting that significantly impacts performance
please contact trapexit so he may investigate further. Please test
both against your normal setup, a singular branch, and with
`nullrw=true`

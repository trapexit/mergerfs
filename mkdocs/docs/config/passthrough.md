# passthrough

* default: `off`
* arguments:
    * `off`: Passthrough is never enabled.
    * `ro`: Only enable passthrough when file opened for reading only.
    * `wo`: Only enable passthrough when file opened for writing only.
    * `rw`: Enable passthrough when file opened for reading, writing,
      or both.

In [Linux 6.9](https://kernelnewbies.org/Linux_6.9#Faster_FUSE_I.2FO)
a IO passthrough feature was added to FUSE. Typically `mergerfs` has
to act as an active proxy for all read and write requests. This
results in, at times, significant overhead compared to direct
interaction with the underlying filesystems. Not because `mergerfs` is
doing anything particularly slow but because the additional
communication and data transfers required. With the `passthrough`
feature `mergerfs` is able to instruct the kernel to perform reads and
writes directly on the underlying file. Bypassing `mergerfs` entirely
(specifically and only for reads and writes) and thereby providing
near native performance.

This performance does come at the cost of some functionality as
`mergerfs` no longer has control over reads and writes which means all
features related to read/write are affected.

* [moveonenospc](moveonenospc.md): Does not work because errors are
  not reported back to `mergerfs`.
* nullrw: Since `mergerfs` no longer receives read/write requests
  there is no read or write to ignore.
* [readahead](readahead.md): Still affects the `readahead` values of
  underlying filesystems and `mergerfs` itself but no longer relevant
  to `mergerfs` in that it is not servicing IO requests.
* [fuse_msg_size](fuse_msg_size.md): The primary reason to increase
  the FUSE message size is to allow transferring more data per request
  which helps improve read and write performance. Without read/write
  requests being sent to `mergerfs` there is little reason to have
  larger message sizes since the only other message larger than 1
  page, directory reading, currently has a small fixed buffer size.
* parallel-direct-writes: Not only is `direct-io` and `passthrough`
  mutually exclusive but since `mergerfs` no longer receives write
  requests in passthrough mode then there is no parallel writes
  possible.
* [cache.writeback](cache.md): FUSE's writeback caching is
  incompatible with `passthrough`. If `cache.writeback=true` when
  enabling `passthrough` it will be reset to `false`.
* [cache.files](cache.md): Must be enabled for `passthrough` to
  work. When `cache.files=off` FUSE's `direct-io` mode is enabled
  which overrides `passthrough.` Meaning `cache.files` should be set
  to `partial`, `full` or `auto-full` to use `passthrough`. If
  `cache.files` is set to `off` when using `passthrough` it will
  reset it to `auto-full`.

Technically `mergerfs` has the ability to choose options like
`passthrough`, `direct-io`, and page caching independently for every
file opened. However, at the moment there is no use case for picking
and choosing which to enable outside `cache.files=per-process` (which
is largely unnecessary on Linux v6.6 and above. See
[direct-io-allow-mmap](options.md)) If such a use case arises please
reach out to the author to discuss.

Unlike [preload.so](../tooling.md#preloadso), `passthrough` will work for
any software interacting with `mergerfs`. However, `passthrough`
requires Linux v6.9 or above to work.

**NOTE:** This feature will **ONLY** work if `mergerfs` is running as
`root` as currently only `root` is allowed to leverage the kernel
feature.

**NOTE:** If a file has been opened and passthrough enabled, while that
file is open, if another open request is made `mergerfs` must also
enable `passthrough` for the second open request. This is a limitation
of how the passthrough feature works.


## Alternatives

* [preload.so](../tooling.md#preloadso): Leverages the ability to
  intercept certain filesystem calls to bypass `mergerfs` for
  IO. See page for details and limitations.


## Benchmarks

Simple `dd` based benchmark on a Hades Canyon (Intel i7-8809G, 32GB
DDR4-2400 RAM).

```
# /mnt/test is mergerfs over a single tmpfs mount
dd if=/dev/zero of=/mnt/test/tmpfile bs=1M count=2000 oflag=nocache
conv=fdatasync status=progress
```

Summary: passthrough is ~95% native speed. 200% faster than
`direct-io` (cache.files=off).

* straight to tmpfs: 1.7 GB/s
* nullrw=true: 2.1 GB/s 
* cache.files=off; passthrough=off: 800 MB/s
* cache.files=auto-full; passthrough=rw: 1.6 GB/s
* cache.files=auto-full; cache.writeback=false: 518 MB/s
* cache.files=auto-full; cache.writeback=true: 518 MB/s

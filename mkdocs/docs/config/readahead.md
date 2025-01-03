# readahead

Sets the mergerfs and underlying filesystem `readahead` values. The
value unit is in kibibytes.

* `readahead=1024`

While the max size of messages sent between the kernel and mergerfs is
configurable via the [fuse_msg_size](fuse_msg_size.md) option that
doesn't mean that is the size used by the kernel for read and
writes.

Linux has a max read/write size of 2GB. Since the max FUSE message
size is just over 1MB the kernel will break up read and write requests
with buffers larger than that 1MB.

When page caching is disabled (`cache.files=off`), besides the kernel
breaking up requests with larger buffers, requests are effectively one
for one to mergerfs. A read or write request for X bytes is made to
the kernel and a request for X bytes is made to mergerfs. No
[readahead](https://en.wikipedia.org/wiki/Readahead) behavior will
occur because there is no page cache available for it to store that
data. In FUSE this is referred to as "direct IO". Note that "direct
IO" is not the same as `O_DIRECT`.

When page caching is enabled the kernel can and will utilize
`readahead`. However, there are two values which impact the size of
the `readahead` requests. The filesystem's `readahead` value and the
FUSE `max_readahead` value. Whichever is lowest is used. The default
`max_readahead` in mergerfs is maxed out meaning only the filesystem
`readahead` value is relevant.

Preferably this value would be set by the user externally since it is
a generic feature but there is no standard way to do so mergerfs added
this feature to make it easier to set.

There is currently no way to set separate values for different
branches through mergerfs.

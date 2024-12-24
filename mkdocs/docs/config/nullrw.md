# nullrw

* `nullrw=true|false`
* Defaults to `false`.

Due to how FUSE works there is an overhead to all requests made to a
FUSE filesystem that wouldn't exist for an in kernel one. Meaning that
even a simple passthrough will have some slowdown. However, generally
the overhead is minimal in comparison to the cost of the underlying
I/O. By disabling the underlying I/O we can test the theoretical
performance boundaries.

By enabling `nullrw` mergerfs will work as it always does **except**
that all reads and writes will be no-ops. A write will succeed (the
size of the write will be returned as if it were successful) but
mergerfs does nothing with the data it was given. Similarly a read
will return the size requested but won't touch the buffer.

See the [benchmarking](../benchmarking.md) section for suggestions on
how to test.

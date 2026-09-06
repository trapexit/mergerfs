# splice

* type: `BOOL`
* default: `true`
* example: `splice=false`

Controls whether mergerfs uses the kernel's
[splice](https://man7.org/linux/man-pages/man2/splice.2.html) syscall
to move data between `/dev/fuse` and the underlying filesystems
without copying it through user space memory.

When enabled, `read` and `write` requests can be serviced with zero
copies: the kernel moves pages directly between the branch
filesystem's page cache and the FUSE device through pipes. This can
significantly improve sequential throughput on modern hardware.

Due to the extra syscall overhead for small requests, splice replies
to `read` requests are only used for requests of 128KiB or larger;
smaller read replies use the traditional write path. `write` requests
of any size stage their payload into the receive pipe and splice it
to the branch when possible.

If the filesystem or kernel does not support the needed
functionality mergerfs will automatically fall back to the
traditional read/write behavior.

Historical note: splice support was previously removed from
mergerfs's own vendored libfuse (see the
[FAQ](../faq/technical_behavior_and_limitations.md) for why), and
the `no-splice-read`, `no-splice-write`, and `no-splice-move`
spellings were accepted as no-ops during that period. They remain
accepted as no-ops for backward compatibility; use `splice=false`
instead.

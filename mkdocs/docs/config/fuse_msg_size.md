# fuse_msg_size

* `fuse_msg_size=UINT|SIZE`
* Defaults to `1M`
* Performance improvements often peak at about `4M`

FUSE applications communicate with the kernel over a special character
device: `/dev/fuse`. A large portion of the overhead associated with
FUSE is the cost of going back and forth between user space and kernel
space over that device. Generally speaking, the fewer trips needed the
better the performance will be. Reducing the number of trips can be
done a number of ways. Kernel level caching and increasing message
sizes being two significant ones. When it comes to reads and writes if
the message size is doubled the number of trips are approximately
halved.

In Linux v4.20 a new feature was added allowing the negotiation of the
max message size. Since the size is in multiples of
[pages](https://en.wikipedia.org/wiki/Page_(computer_memory)) the
feature is called `max_pages`. In versions of Linux prior to v6.13
there is a maximum `max_pages` value of 256 (1MiB) and minimum of 1
(4KiB). In [Linux
v6.13](https://web.git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=2b3933b1e0a0a4b758fbc164bb31db0c113a7e2c)
and above the max value supported by the kernel can range from 1
(4KiB) to 65535 (~256MiB) (assuming a page size of 4KiB.) The default
used by Linux >= 4.20, and hard coded value used before 4.20, is 32
(128KiB). In mergerfs it is referred to as `fuse_msg_size` to make it
clear what it impacts and provide some abstraction.

If the `fuse_msg_size` value provided is more than the system wide
maximum mergerfs will attempt to increase the system wide value to keep
the user from needing to set the value using `sysctl`,
`/etc/sysctl.conf`, or `/proc/sys/fs/fuse/max_pages_limit`.

The main downside to increasing the value is that memory usage will
increase approximately relative to the number of [processing
threads](threads.md) configured. Keep this in mind for systems with
lower amounts of memory like most SBCs. Performance improvements seem
to peak around 4MiB.

On kernels before v4.20 the option has no effect. On kernels between
v4.20 and v6.13 the max value is 256. On kernels >= v6.13 the maximum
value is 65535.

Since page size can differ between systems mergerfs can take a value in
bytes and will convert it to the proper number of pages (rounded up).

NOTE: If you intend to enable `cache.files` you should also set
[readahead](readahead.md) to match `fuse_msg_size`.

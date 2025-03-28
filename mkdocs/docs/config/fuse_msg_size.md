# fuse_msg_size

* `fuse_msg_size=UINT|SIZE`
* Defaults to `1M`

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
(4KiB). In Linux v6.13 and above the max value supported by the kernel
can range from 1 (4KiB) to 65535 (~256MiB). The default used by Linux
>=4.20, and hardcoded value used before 4.20, is 32 (128KiB). In
mergerfs it's referred to as fuse_msg_size to make it clear what it
impacts and provide some abstraction.

If the `fuse_msg_size` value provided is less than the systemwide
maximum mergerfs will attempt to increase the systemwide value to keep
the user from needing to set the value using `sysctl` or
`/etc/sysctl.conf`.

The main downside to increasing the value is that memory usage will
increase approximately relative to the number of [processing
threads](threads.md) configured. Keep this in mind for systems with
lower amounts of memory like most SBCs.

On kernels before v4.20 the value has no effect. On kernels between
v4.20 and v6.13 it will ignore values above 256. On kernels >= v6.13
values above 65535 will be set to 65535.

# fuse_msg_size

* `fuse_msg_size=UINT`
* Defaults to `256`

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
feature is called `max_pages`. There is a maximum `max_pages` value of
256 (1MiB) and minimum of 1 (4KiB). The default used by Linux >=4.20,
and hardcoded value used before 4.20, is 32 (128KiB). In mergerfs it's
referred to as fuse_msg_size to make it clear what it impacts and
provide some abstraction.

Since there should be no downsides to increasing `fuse_msg_size`,
outside a minor increase in RAM usage due to larger message buffers,
mergerfs defaults the value to 256. On kernels before v4.20 the value
has no effect. The reason the value is configurable is to enable
experimentation and benchmarking.

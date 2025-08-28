# proxy-ioprio

* type: `BOOL`
* default: `false`
* example: `proxy-ioprio=true`

In Linux certain process schedulers have the ability to
[prioritize](https://man7.org/linux/man-pages/man2/ioprio_set.2.html)
based on a per thread [IO
priority](https://www.kernel.org/doc/Documentation/block/ioprio.txt)
assigned to the software by users using tools such as
[ionice](https://man7.org/linux/man-pages/man1/ionice.1.html). Since
such details are not provided by the FUSE protocol, users may not use
schedulers which support IO priority, and the extra overhead to query
the priority and set it within mergerfs it does not by default attempt
to mirror/proxy the value. When enabled however, for all read and
write requests, mergerfs will query the priority of the requesting
process/thread and set the same value on the thread within mergerfs
making the read/write.

This may be useful in situation where the system has high IO load or
processes are known to have varying priorities. See the [man
page](https://man7.org/linux/man-pages/man2/ioprio_set.2.html) for
specific details.

Keep in mind that if no IO scheduler has been set for a thread then by
default the IO priority will match the CPU nice value which for
mergerfs is set by [scheduling-priority](options.md). [Nice
value](https://man7.org/linux/man-pages/man2/getpriority.2.html)
proxying may be added in a future release.


## Conflicts With Other Options

If using [IO passthrough](passthrough.md) there is no reason to set
this value to `true`. However, since `passthrough` leads to mergerfs
IO calls being bypassed entirely will have no impact on performance or
behavior regardless.

When using `passthrough` the IO priority of the client app would be
used directly given the IO is passed through.


## Supported Platforms

Only Linux. This is not supported on FreeBSD.


## Performance Impact

Despite the additional syscalls requires the impact appears
undetectable with larger buffer sizes. At very small buffer sizes
(such as 512 bytes) there is a noticable but small impact (~1.5%).

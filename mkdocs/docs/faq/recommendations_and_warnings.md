# Recommendations and Warnings

## What should mergerfs NOT be used for?

- databases: Even if the database stored data in separate files
  (mergerfs wouldn't offer much otherwise) the higher latency of the
  indirection will really harm performance. If it is a lightly used
  sqlite3 database then it should be fine.
- VM images: For the same reasons as databases. VM images are accessed
  very aggressively and mergerfs will introduce a lot of extra latency.
- As replacement for RAID: mergerfs is just for pooling branches. If
  you need that kind of device performance aggregation or high
  availability you should stick with RAID. However, it is fine to put
  a filesystem which is on a RAID setup in mergerfs.


## It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?

[mhddfs](https://github.com/trapexit/mhddfs) manages running as
`root` by calling
[getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319)
and if it returns `0` then it will
[chown](http://linux.die.net/man/1/chown) the file. Not only is that a
race condition but it doesn't handle other situations. Rather than
attempting to simulate POSIX ACL behavior the proper way to manage
this is to use [seteuid](http://linux.die.net/man/2/seteuid) and
[setegid](http://linux.die.net/man/2/setegid), in effect, becoming the
user making the original call, and perform the action as them. This is
what mergerfs does and why mergerfs should always run as root.

In Linux setreuid syscalls apply only to the thread. glibc hides this
away by using realtime signals to inform all threads to change
credentials. Taking after Samba, mergerfs uses
`syscall(SYS_setreuid,...)` to set the callers credentials for that
thread only. Jumping back to `root` as necessary should escalated
privileges be needed (for instance: to clone paths between
filesystems).

For non-Linux systems, mergerfs uses a read-write lock and changes
credentials only when necessary. If multiple threads are to be user X
then only the first one will need to change the processes
credentials. So long as the other threads need to be user X they will
take a readlock allowing multiple threads to share the
credentials. Once a request comes in to run as user Y that thread will
attempt a write lock and change to Y's credentials when it can. If the
ability to give writers priority is supported then that flag will be
used so threads trying to change credentials don't starve. This isn't
the best solution but should work reasonably well assuming there are
few users.

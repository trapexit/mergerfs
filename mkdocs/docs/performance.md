# Tweaking Performance

**mergerfs** is effectively a filesystem proxy and therefore its
theoretical max performance is that of the underlying devices
(ignoring caching performed by the kernel.) However, given it is a
FUSE based filesystem working from userspace and it must combine the
behavior and information from multiple underlying branches there can
be an increase in overhead relative to other solutions. That said the
performance of certain functions, such as IO, can match the
theoretical max but it depends greatly on the system's
configuration. There are many things which can impact
performance. Device speeds and latency, network speeds and latency,
concurrency and parallel limits of the hardware, read/write sizes, the
number of branches, etc.

While some settings can impact performance they are all **functional**
in nature. Meaning they change mergerfs' behavior in some way. As a
result there is really no such thing as a "performance mode".

If you're having performance concerns please read over the
[benchmarking section](benchmarking.md) of these docs and then the
details below.

NOTE: Be sure to [read about available features](config/options.md)
before changing them to understand how functionality will change.

* test theoretical performance using `nullrw` or using a ram disk as a
  branch
* enable [passthrough.io](config/passthrough.md) (likely to have the
  biggest impact)
* change read or process [thread pools](config/threads.md)
* change [func.readdir](config/func_readdir.md)
* increase [readahead](config/readahead.md): `readahead=1024`
* disable `security-capability` and/or [xattr](config/xattr.md)
* increase cache timeouts [cache.attr](config/cache.md#cacheattr),
  [cache.entry](config/cache.md#cacheentry),
  [cache.negative-entry](config/cache.md#cachenegative-entry)
* toggle [page caching](config/cache.md#cachefiles)
* enable `parallel-direct-writes`
* enable [cache.statfs](config/cache.md#cachestatfs)
* enable [cache.symlinks](config/cache.md#cachesymlinks)
* enable [cache.readdir](config/cache.md#cachereaddir)
* disable `posix-acl`
* disable `async-read`
* use [symlinkify](config/symlinkify.md) if your data is largely
  static and read-only
* use [tiered cache](extended_usage_patterns.md) devices
* use LVM and LVM cache to place a SSD in front of your HDDs


## Understanding mergerfs IO Performance

A common source of confusion when evaluating **mergerfs**' performance
is comparing it to traditional RAID or filesystem level pooling
technologies such as mdadm, ZFS, btrfs, or striped LVM. Those
solutions aggregate devices into a single virtual device and spread IO
across all members. Under favorable conditions that means the
performance of the pool is the sum of the performance of its members:
striped reads and writes hit multiple drives at once and the aggregate
throughput scales with the number of devices, up to the limits of the
interconnects (PCIe, SATA, network, etc.) which every member must
share.

**mergerfs** does not aggregate or stripe data. Every file lives
wholly on one branch and **mergerfs** simply routes access to it. As a
result **mergerfs** does not aggregate the performance of its branches
in the way RAID does. What it provides instead is **independent
performance across branches**.

* **Best case:** workloads which access different branches
  concurrently perform independently of one another. If you have 3
  filesystems on 3 drives and each is being actively used, the total
  throughput can be A + B + C: the sum of each branch performing at
  full speed. No single drive is a bottleneck for the others.
* **Worst case:** a single stream of IO - one process reading or
  writing one file - performs at the speed of the single branch the
  file is on, no differently than a lone filesystem. Additional
  branches do not make a single file faster.

This means that when reasoning about expected performance the key
question is whether your workload is **single stream** or **multi
stream**:

* Single large file transfers, video playback, or one process doing
  sequential IO will never exceed the speed of one branch regardless
  of how many are mounted. If a single stream must be faster, put a
  faster device (tiered cache, SSD, LVM cache) in front of the
  branches or use an actual striped RAID layer beneath **mergerfs**'
  branches.
* Concurrency across branches is where **mergerfs** shines. Many
  parallel downloads, seeding torrents, multiple rsync streams,
  multiple applications reading different files: these naturally
  spread across branches and their throughput adds up.

There are practical upsides to this non-aggregated design as well:

* Drives of different sizes and speeds don't reduce the whole pool to
  the slowest member. Each branch performs at its own speed.
* The failure of one drive only affects the files on that branch
  rather than the integrity of the entire pool.

Finally, keep in mind that **mergerfs** is a proxy above the branches,
so measured throughput is the lesser of the branch's native performance
and any overhead introduced by **mergerfs** and FUSE. Use
[nullrw](config/options.md) or a ram disk as a branch to measure that
overhead separately, and see the [benchmarking
section](benchmarking.md) for how to benchmark meaningfully.


## Additional Reading

* [Benchmarking](benchmarking.md)
* [Options](config/options.md)
* [Tips and Notes](tips_notes.md)

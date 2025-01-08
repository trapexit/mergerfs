# Usage Patterns

## tiered cache

Some storage technologies support what is called "tiered" caching. The
placing of smaller, faster storage as a transparent cache to larger,
slower storage. NVMe, SSD, Optane in front of traditional HDDs for
instance.

mergerfs does not natively support any sort of tiered caching. Most
users have no use for such a feature and its inclusion would
complicate the code as it exists today. However, there are a few
situations where a cache filesystem could help with a typical mergerfs
setup.

1.  Fast network, slow filesystems, many readers: You've a 10+Gbps
    network with many readers and your regular filesystems can't keep
    up.
2.  Fast network, slow filesystems, small'ish bursty writes: You have
    a 10+Gbps network and wish to transfer amounts of data less than
    your cache filesystem but wish to do so quickly and the time
    between bursts is long enough to migrate data.

With #1 it's arguable if you should be using mergerfs at all. A RAID
level that can aggregate performance or using higher performance
storage would probably be the better solution. If you're going to use
mergerfs there are other tactics that may help: spreading the data
across filesystems (see the mergerfs.dup tool) and setting
`func.open=rand`, using `symlinkify`, or using dm-cache or a similar
technology to add tiered cache to the underlying device itself.

With #2 one could use dm-cache as well but there is another solution
which requires only mergerfs and a cronjob.

1.  Create 2 mergerfs pools. One which includes just the slow branches
    and one which has both the fast branches (SSD,NVME,etc.) and slow
    branches. The 'base' pool and the 'cache' pool.
2.  The 'cache' pool should have the cache branches listed first in
    the branch list.
3.  The best `create` policies to use for the 'cache' pool would
    probably be `ff`, `epff`, `lfs`, `msplfs`, or `eplfs`. The latter
    three under the assumption that the cache filesystem(s) are far
    smaller than the backing filesystems. If using path preserving
    policies remember that you'll need to manually create the core
    directories of those paths you wish to be cached. Be sure the
    permissions are in sync. Use `mergerfs.fsck` to check / correct
    them. You could also set the slow filesystems mode to `NC` though
    that'd mean if the cache filesystems fill you'd get "out of space"
    errors.
4.  Enable `moveonenospc` and set `minfreespace` appropriately. To
    make sure there is enough room on the "slow" pool you might want
    to set `minfreespace` to at least as large as the size of the
    largest cache filesystem if not larger. This way in the worst case
    the whole of the cache filesystem(s) can be moved to the other
    drives.
5.  Set your programs to use the 'cache' pool.
6.  Save one of the below scripts or create you're own. The script's
    responsibility is to move files from the cache filesystems (not
    pool) to the 'base' pool.
7.  Use `cron` (as root) to schedule the command at whatever frequency
    is appropriate for your workflow.


### time based expiring

Move files from cache to base pool based only on the last time the
file was accessed. Replace `-atime` with `-amin` if you want minutes
rather than days. May want to use the `fadvise` / `--drop-cache`
version of rsync or run rsync with the tool
[nocache](https://github.com/Feh/nocache).

**NOTE:** The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.time-based-mover](https://github.com/trapexit/mergerfs/blob/latest-release/tools/mergerfs.time-based-mover?raw=1)


### percentage full expiring

Move the oldest file from the cache to the backing pool. Continue till
below percentage threshold.

**NOTE:** The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.percent-full-mover](https://github.com/trapexit/mergerfs/blob/latest-release/tools/mergerfs.percent-full-mover?raw=1)

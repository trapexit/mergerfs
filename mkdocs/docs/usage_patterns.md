# Usage Patterns

## tiered cache

Some storage technologies support what is called "tiered" caching. The
placing of smaller, faster storage as a transparent cache to larger,
slower storage. NVMe, SSD, or Optane in front of traditional HDDs for
instance.

mergerfs does not natively support any sort of tiered caching
currently. The truth is for many users a cache would have little or no
advantage over reading and writing directly. They would be
bottlenecked by their network, internet connection, or limited size of
the cache. However, there are a few situations where a tiered cache
setup could help.

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

With #2 one could use a block cache solution as available via LVM and
dm-cache but there is another solution which requires only mergerfs, a
script to move files around, and a cron job to run said script.

* Create two mergerfs pools. One which includes just the **slow**
  branches and one which has **both** the **fast** branches
  (SSD,NVME,etc.) and **slow** branches. The **base** pool and the
  **cache** pool.
* The **cache** pool should have the cache branches listed first in
  the branch list in order to to make it easier to prioritize them.
* The best `create` policies to use for the **cache** pool would
  probably be `ff`, `lus`, or `lfs`. The latter two under the
  assumption that the cache filesystem(s) are far smaller than the
  backing filesystems.
* You can also set the **slow** branches' mode to `NC` which would
  give you the ability to use other `create` policies though that'd
  mean if the cache filesystems fill you'd get "out of space"
  errors. This however may be good as it would indicate the script
  moving files around is not configured properly.
* Set your programs to use the **cache** pool.
* Configure the **base** pool with the `create` policy you would like
  to lay out files as you like.
* Save one of the below scripts or create your own. The script's
  responsibility is to move files from the **cache** branches (not
  pool) to the **base** pool.
* Use `cron` (as root) to schedule the command at whatever frequency
  is appropriate for your workflow.


### time based expiring

Move files from cache filesystem to base pool which have an access
time older than the supplied number of days. Replace `-atime` with
`-amin` in the script if you want minutes rather than days.

**NOTE:** The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.time-based-mover](https://github.com/trapexit/mergerfs/blob/latest-release/tools/mergerfs.time-based-mover?raw=1)

Download:
```
curl -o /usr/local/bin/mergerfs.time-based-mover https://raw.githubusercontent.com/trapexit/mergerfs/refs/heads/latest-release/tools/mergerfs.time-based-mover
```

crontab entry:
```
# m h  dom mon dow   command
0 * * * * /usr/local/bin/mergerfs.time-based-mover /mnt/ssd/cache00 /mnt/base-pool 1
```

If you have more than one cache filesystem then simply add a cron
entry for each.

If you want to only move files from a subdirectory then use the
subdirectories. `/mnt/ssd/cache00/foo` and `/mnt/base-pool/foo`
respectively.


### percentage full expiring

While the cache filesystem's percentage full is above the provided
value move the oldest file from the cache filesystem to the base pool.

**NOTE:** The arguments to these scripts include the cache
**filesystem** itself. Not the pool with the cache filesystem. You
could have data loss if the source is the cache pool.

[mergerfs.percent-full-mover](https://github.com/trapexit/mergerfs/blob/latest-release/tools/mergerfs.percent-full-mover?raw=1)

Download:
```
curl -o /usr/local/bin/mergerfs.percent-full-mover https://raw.githubusercontent.com/trapexit/mergerfs/refs/heads/latest-release/tools/mergerfs.percent-full-mover
```

crontab entry:
```
# m h  dom mon dow   command
0 * * * * /usr/local/bin/mergerfs.percent-full-mover /mnt/ssd/cache00 /mnt/base-pool 80
```

If you have more than one cache filesystem then simply add a cron
entry for each.

If you want to only move files from a subdirectory then use the
subdirectories. `/mnt/ssd/cache00/foo` and `/mnt/base-pool/foo`
respectively.

# Limiting drive spinup

## Does mergerfs do anything to prevent drive spinup?

No. In fact it makes it worse.


## How can I setup my system to limit drive spinup?

TL;DR: You really can't. Not through mergerfs alone. In fact using
mergerfs makes any general attempt to limit spinup more complicated.


mergerfs is a proxy. Despite having [some caching
behaviors](../config/cache.md) it is not designed to cache much more
than metadata. It proxies calls between client software and underlying
filesystems. If a client makes a request such as `open`, `readdir`,
`stat`, etc. it must translate that into something that makes sense
across multiple filesystems. For `readdir` that means running the same
call against all branches and aggregating the output. For `open` that
means finding the file to open and doing so. The only way to find the
file to open is to scan across all branches and sort the results and
pick one. There is some variability in how much searching is done due
to
[policies](../config/functions_categories_policies.md#policy-descriptions)
but not much. There is no practical way to do otherwise. Especially
given so many mergerfs users expect out-of-band changes to "just
work."

The best way to limit spinup of drives is to limit their usage at the
client level. Meaning keeping software from interacting with the
filesystem (and therefore the drive) all together.


### What if you assume no out-of-band changes and cache everything?

This would require a significant rewrite of mergerfs. Everything is
done on the fly right now and all those calls to underlying
filesystems can cause a spinup. To work around that a database of some
sort would have to be used to store ALL metadata about the underlying
filesystems and on startup everything scanned and stored. From then on
it would have to carefully update all the same data the filesystems
do. It couldn't be kept in RAM because it would take up too much space
so it'd have to be on a SSD or other storage device. If anything
changed out of band it would break things in weird ways (and MANY
users depend on out of band changes working.) It could rescan on
occasion but that would require spinning up everything. Filesystem
watches could be used to get updates when the filesystem changes but
that would allow for subtle race conditions and might keep the drives
from spinning down. Also, since mergerfs is just another piece of
software interacting with the filesystem ALL mergerfs requests would
be echoed back to it causing lots of overhead throwing away a super
majority of events it triggered. And something as "simple" as keeping
the current available free space on each filesystem is not as easy as
one might think given reflinks, snapshots, and other block level dedup
technologies distort the meaning of usage values.

Only if ALL metadata and all file locations were cached could mergerfs
then do very targeted file operations so as not to trigger a scan
across the pool. But that would break out of band interactions. It
would require accepting possibly bad behavior by space based policies
since space would be imprecisely computed. It would require long scans
of the filesystems on startup. It would require fundamental changes to
how mergerfs works.


### What if you only query already active drives?

Let's assume that is plausible (it isn't because some drives actually
will spin up if you ask if they are spun down... yes... really) you
would have to either cache all the metadata on the filesystem or treat
it like the filesystem doesn't exist. The former has all the problems
mentioned prior and the latter would break a lot of things. Suddenly
whole filesystems worth of data would be missing because it spun down.


### Is there anything that can be done where mergerfs is involved?

Yes, but whether it works for you depends on your tolerance for the
complexity.

1. Cleanly separate writing, storing, and consuming the data.
   1. Use a SSD or dedicated and limited pool of drives for downloads / torrents.
   2. When downloaded move the files to the primary storage pool.
   3. When setting up software like Plex, Jellyfin, etc. point to the
      underlying filesystems. Not mergerfs.
2. Add a bunch of bcache, lvmcache, dm-cache, or similar block level
   cache to your setup. After a bit of use, assuming sufficient
   storage space, you can limit the likelihood of the underlying
   spinning disks from needing to be hit.

Remember too that while it may be a trade off you are willing to live
with there is decent evidence that spinning down drives puts increased
wear on them and can lead to their death earlier than otherwise. You
may end up saving money on electricity but spending more on having to
purchase new drives.

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
watches could be used but since mergerfs is just another piece of
software interacting with the filesystem ALL mergerfs requests would
be echoed back to it causing lots of overhead throwing away a super
majority of events it triggered. And something as "simple" as keeping
the current available free space on each filesystem is not as easy as
one might think given reflinks, snapshots, and other block level dedup
technologies distort the meaning of usage values.

Only if ALL metadata and all file locations were cached could mergerfs
then do very targeted file operations so as not to trigger a scan
across the pool. But that will break out of band interactions. It
would require accepting possibly bad behavior by space based policies
since space would be imprecisely computed. It would require long scans
of the filesystems on startup and a separate location to store all
this data.


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
3. Tweak your system's settings around caching:
    * mount all your filesystems with `noatime` to disable updating
      "last accessed" time on files.
    * mount all your filesystems with `nodiratime` to disable updating
      "last accessed" time on directories.
    * `sudo sysctl -w vm.vfs_cache_pressure=10`: Prioritizes keeping
      file metadata cached in RAM.
    * `sudo sysctl -w vm.dirty_ratio=40`: Allows up to 40% of system
      memory to be filled with unwritten "dirty" data before forcing a
      disk flush.
    * `sudo sysctl -w vm.dirty_background_ratio=5`: Sets a lower
      threshold (5%) for the kernel to start writing data in the
      background without blocking applications.
    * `sudo sysctl -w vm.dirty_writeback_centisecs=6000`: Increases
      the interval between kernel writeback checks to 60 seconds
      (default is usually 5).
    * `sudo sysctl -w vm.dirty_expire_centisecs=12000`: Allows data to
      stay "dirty" in RAM for up to 120 seconds before it must be
      written to disk.
    * To make `sysctl` values permanent add the above to
      `/etc/sysctl.conf`.
    * **Note:** for systems with limited RAM the above `sysctl` values
      could cause issues.
    * **Note:** if you increase the time for which data is stored in
      RAM before being flushed to disk there is also an increased risk
      that a sudden powerloss could lead to lost data.
4. Enable all the appropriate caches in mergerfs:
    * [https://trapexit.github.io/mergerfs/latest/config/cache/](https://trapexit.github.io/mergerfs/latest/config/cache/)
    * `cache.entry=86400`: cache file existence for 24h.
    * `cache.negative-entry=86400`: cache file non-existence for 24h.
    * `cache.attr=86400`: cache file metadata for 24h.
    * `cache.symlinks=true`: enable symlink caching.
    * `cache.readdir=true`: enable directory listing caching.


### References

* [Documentation for /proc/sys/vm/](https://docs.kernel.org/admin-guide/sysctl/vm.html)
* [How to Configure vm.swappiness and Other Virtual Memory Parameters on RHEL](https://oneuptime.com/blog/post/2026-03-04-configure-vm-swappiness-virtual-memory-rhel-9/view)


### Tradeoffs

While it may be a trade off you are willing to live with there is
decent evidence that spinning down drives puts material increased wear
on them and can lead to their death earlier than otherwise. You may
end up saving money on electricity but spending more on having to
purchase new drives sooner.


## Alternatives

[Project Comparisons](../project_comparisons.md) lists a few projects
that are similar to mergerfs but offer caching. Please note that these
projects are very young and not general purpose like mergerfs. Their
compliance to POSIX filesystem rules is probably low enough that
certain behaviors may not work or worse lead to data corruption.

I'm not suggesting not to look at those projects but filesystem
development is complicated even when it appears to be just a layer on
top of another. Just be careful and do your research. That stands for
anything touching your data... mergerfs included.

To the degree that mergerfs can add features which help in this
situation, so long as it doesn't interfere with other fundamental
features, they may be added over time.

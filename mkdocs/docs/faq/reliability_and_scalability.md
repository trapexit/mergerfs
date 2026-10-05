# Reliability and Scalability

## Is mergerfs "production ready?"

Yes.

mergerfs has been around for over a decade and used by many users on
their systems. Typically running 24/7 with constant load.

At least a few companies are believed to use mergerfs in production
environments. A number of [NAS focused operating
systems](../related_projects.md) includes mergerfs as a solution for
pooling filesystems.

Most serious issues (crashes or data corruption) have been due to
[kernel bugs](../known_issues_bugs.md#fuse-and-linux-kernel). All of
which are fixed in stable releases.


## How well does mergerfs scale?

Users have reported running mergerfs on everything from OpenWRT
routers and Raspberry Pi SBCs to multi-socket Xeon enterprise servers.

Users have pooled everything from USB thumb drives to enterprise NVME
SSDs to remote filesystems and rclone mounts.

The cost of many calls can be `O(n)` meaning adding more branches to
the pool will increase the cost of certain functions, such as reading
directories or finding files to open, but there are a number of caches
and strategies in place to limit overhead where possible.


## Are there any limits?

There is no maximum capacity beyond what is imposed by the operating
system itself. Any limit is practical rather than technical. As
explained in the question about scale mergerfs is mostly limited by
the tolerated cost of aggregating branches and the cost associated
with interacting with them. If you pool slow network filesystem then
that will naturally impact performance more than low latency SSDs.


## Should I avoid SMR (shingled) drives in a mergerfs pool?

mergerfs itself is only a proxy and has no opinion about the recording
technology of the underlying drives — on its own it neither helps nor
hurts an SMR (shingled) drive.

The caveat is the workloads mergerfs pools are commonly paired
with. SMR drives rewrite overlapping tracks and absorb bursts in a
small CMR cache zone; once that zone fills under sustained random or
large sequential writes, throughput can collapse to single-digit MB/s
until the drive reorganizes itself. Two common mergerfs patterns hit
exactly that:

* **[SnapRAID](compatibility_and_integration.md#can-i-use-mergerfs-without-snapraid-snapraid-without-mergerfs)
  parity**: a full `sync` or `scrub` writes large, sustained streams
  to the parity drive(s). On an SMR parity drive this can stretch a
  sync from hours into days or cause timeouts.
* **Rebalancing** (e.g. `mergerfs.balance` moving files between
  branches to even out used space — see [tooling](../tooling.md))
  produces the same sustained write pattern on the destination drive.

For pool members, and especially for parity drives, prefer CMR
(conventional) drives. SMR is generally fine for cold,
write-once-read-many data that is not re-synced or rebalanced. Drive
data sheets rarely state the recording technology outright, so
community-maintained CMR/SMR lists and per-capacity
[price-per-terabyte comparisons](https://hddhunt.com/cheapest-hdd-per-tb/)
are useful when sizing large CMR drives for a pool.

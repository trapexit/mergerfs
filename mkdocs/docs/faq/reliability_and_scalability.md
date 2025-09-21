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

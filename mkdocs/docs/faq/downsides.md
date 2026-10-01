# Downsides

## What are the downsides of using mergerfs?

mergerfs trades the features of an integrated storage system for the
flexibility of pooling independent filesystems. It is a filesystem
proxy, not RAID or a replacement for the underlying filesystems.

Some limitations are inherent to that design; others matter only for
particular workloads, configurations, or applications. Many can be
mitigated with appropriate settings or separate tools. The following
is a checklist of tradeoffs to evaluate, not a list of problems every
installation will experience.

### No built-in data protection or storage management

mergerfs does not provide redundancy, parity, integrity checks,
snapshots, or file versioning. If a drive fails, mergerfs cannot
recover the files on it. The other drives retain their independent
filesystems, but that failure isolation is not redundancy or a backup.
Protection must come from the underlying storage or separate tools,
such as [SnapRAID](https://www.snapraid.it) or
[NonRAID](https://github.com/qvr/nonraid).

It also does not automatically rebalance existing files when a branch
is added or space usage changes. If redistribution is needed, it must
be done separately.

See [Non-features](../index.md#non-features), [Project
Comparisons](../project_comparisons.md), and [How can I balance files
across the pool?](configuration_and_policies.md#how-can-i-balance-files-across-the-pool).

### No striping or contiguous pooled space

Each file lives entirely on one branch. Adding drives does not make a
single-file transfer faster, and a file must fit on one eligible
branch even if the pool reports enough total free space. For example,
two filesystems with 6GiB free each cannot hold one 10GiB file.
Concurrent access to different branches can still provide aggregate
throughput.

See [Project Comparisons](../project_comparisons.md#raid0-jbod-span-drive-concatenation-striping)
and [Performance](../performance.md#understanding-mergerfs-io-performance).

### Performance overhead

Filesystem requests generally pass through FUSE and the mergerfs
process before reaching the underlying filesystem. This adds latency
and CPU overhead compared to direct access. Directory listings and
other metadata operations can require work across multiple branches,
so their cost can grow as the pool grows. Page caching can also result
in data being cached both through mergerfs and on the underlying
filesystem.

[IO passthrough](../config/passthrough.md) can reduce data IO overhead,
but does not eliminate the work of combining branches. Databases, VM
images, and other latency-sensitive workloads should be benchmarked
with the intended configuration rather than assumed to perform like
direct access.

There is also memory overhead beyond page caching: request buffers,
temporary directory buffers, and FUSE node bookkeeping. Usage depends
on concurrency, buffer sizes, and the workload. Exporting the pool
over NFS requires retaining nodes with `never-forget-nodes=true`,
which can increase memory usage as more entries are accessed. This is
worth considering on memory-constrained systems or large NFS exports.
See [Resource Usage](../resource_usage.md) and [NFS exporting
mergerfs](../remote_filesystems.md#nfs-exporting-mergerfs).

See [Understanding mergerfs IO
Performance](../performance.md#understanding-mergerfs-io-performance),
[Benchmarking](../benchmarking.md), and [Why use
FUSE?](technical_behavior_and_limitations.md#why-use-fuse-why-not-a-kernel-based-solution).

### Placement and filesystem semantics need attention

A single mount point does not make the branches one filesystem.
[Policies](../config/functions_categories_policies.md) determine where
new files go and which copy is used when the same path exists on
multiple branches. Path-preserving policies and
[minfreespace](../config/minfreespace.md) can exclude branches from file
creation even while the pool has free space. mergerfs cannot know a
file's final size when it is created.

Hard links and renames still have underlying filesystem constraints.
Depending on the layout and policies, an operation can return `EXDEV`
(cross-device link) even within the pool. Applications which do not
handle that error may fail rather than fall back to copying.

Directory metadata is also selected by policy rather than necessarily
aggregated. With the default `getattr` policy of `ff`, a directory's
timestamp comes from the first matching branch. A media scanner which
uses that timestamp to skip unchanged directories can miss updates on
another branch. `func.getattr=newest` can help with timestamp-based
scans, but changes which branch supplies metadata; it does not change
which file other operations select.

Corresponding directories on different branches can have different
ownership or permissions. Since access uses the caller's credentials,
this can result in unexpected access errors or missing entries.
Keeping those permissions consistent and auditing with
`fsck.mergerfs` helps. Container UID/GID mappings and non-POSIX
branches may need additional attention; mergerfs does not bypass
their access restrictions.

See [Configuration and Policies](configuration_and_policies.md),
[rename and link](../config/rename_and_link.md), [Tips and
Notes](../tips_notes.md), and [How does mergerfs interact with user
namespaces?](compatibility_and_integration.md#how-does-mergerfs-interact-with-user-namespaces).

### Keeping drives spun down can be harder

Directory listings and file searches can query multiple branches,
potentially waking drives beyond the one containing the requested
file. If keeping disks asleep is important, this can increase power
usage, noise, and access latency. It is not a concern for SSDs or
drives which are kept spinning.

Caching and reducing background scans can limit some branch access,
but mergerfs alone cannot guarantee that unused disks stay asleep.
See [Limiting drive spinup](limit_drive_spinup.md) for the tradeoffs
and possible approaches.

### Direct branch changes and cached views

Using the underlying filesystems directly is supported, but changes
made outside the mergerfs mount are not forwarded as `inotify` or
`fanotify` events to applications watching the pool. Changes made
through the pool do generate notifications. If files are added
directly to branches, applications such as media servers may need
periodic scans, explicit refreshes, or watches on the branches
themselves.

Metadata caching introduces a separate visibility tradeoff: direct
branch changes may not appear through the pool until cached entries
expire. Longer cache durations can reduce filesystem queries at the
cost of less immediate visibility. Shorter durations or disabling the
relevant caches can help if direct branch changes are common. Avoid
uncoordinated writes to the same file through both paths, especially
when file caching is enabled.

See [Does inotify and fanotify
work?](compatibility_and_integration.md#does-inotify-and-fanotify-work),
[Can filesystems still be used
directly?](usage_and_functionality.md#can-filesystems-still-be-used-directly-outside-of-mergerfs-while-pooled),
and [Caching](../config/cache.md).

### Branch paths do not guarantee a particular disk is mounted

mergerfs works on paths and does not require each branch to be a
mount point. If an intended branch filesystem is not mounted but its
mount-point directory still exists, mergerfs can use that directory
on the filesystem beneath it. If writable and selected for creation,
new files can land there instead of on the intended disk, potentially
filling the root filesystem.

This is a deployment concern when pooling mounted filesystems, not a
problem with intentionally using ordinary directories as branches.
Configure mount dependencies or
`branches-mount-timeout` with `branches-mount-timeout-fail=true` if
startup should fail when branches are not ready. These are startup
safeguards, not continuous checks that the intended disks remain
mounted at runtime.

See [branches-mount-timeout](../config/branches-mount-timeout.md) and
[What happens if a filesystem disappears at
runtime?](usage_and_functionality.md#what-happens-if-a-filesystem-disappears-at-runtime).

### Dependence on branch responsiveness

mergerfs cannot make an unresponsive disk or remote filesystem
responsive. A request to a blocked branch blocks the thread handling
it. If enough threads are blocked, the pool can stop responding as
well. Independent on-disk filesystems do not guarantee that a failing
branch has no effect on access through the pool.

See [What happens if a branch filesystem
blocks?](technical_behavior_and_limitations.md#what-happens-if-a-branch-filesystem-blocks).

### Compatibility limitations

Some filesystem features are unavailable through FUSE or require
particular kernel versions and settings. For example, reflink requests
(`FICLONE` / `FICLONERANGE`) are not supported through the mergerfs
mount. Applications using `mmap` need page caching; older Linux or
mergerfs versions require it to be explicitly enabled.

Advisory locks taken through the pool are not forwarded to the
underlying branches. Local applications using only the mergerfs mount
still have kernel-managed locks, but those locks do not coordinate
with direct branch access or other hosts accessing a remote branch.

Inode identity also has tradeoffs when presenting multiple filesystems
as one. Most applications do not need special settings, but backup or
deduplication tools and NFS can be sensitive to inode values changing
when files move between branches or a policy selects a different
copy. [inodecalc](../config/inodecalc.md) offers different strategies:
for example, `path-hash` stabilizes identity by path for NFS, but
prevents applications from recognizing hard links by inode equality.
The hard links themselves still work.

On SELinux systems, per-file relabeling is unavailable through the
FUSE mount, so container bind-mount options `:z` and `:Z` do not
perform their usual relabeling. This does not mean containers cannot
use the pool: an appropriate SELinux policy or a mount-wide context
can allow access. A mount-wide context is not a substitute for
distinct per-file labels if the application requires them. See [Does
mergerfs work with SELinux
relabeling?](compatibility_and_integration.md#does-mergerfs-work-with-selinux-relabeling).

See [Technical Behavior and
Limitations](technical_behavior_and_limitations.md) and [Known Issues
and Bugs](../known_issues_bugs.md).

These tradeoffs do not require migrating data into a special format.
The underlying filesystems remain usable without mergerfs, and
mergerfs can be [removed without changing their
data](usage_and_functionality.md#can-mergerfs-be-removed-without-affecting-the-data).

# General Information and Overview

## How well does mergerfs scale? Is it "production ready?"

Users have reported running mergerfs on everything from a Raspberry Pi
to dual socket Xeon systems with >20 cores. I'm aware of at least a
few companies which use mergerfs in production. [Open Media
Vault](https://www.openmediavault.org) includes mergerfs as its sole
solution for pooling filesystems. The author of mergerfs had it
running for over 300 days managing 16+ devices with reasonably heavy
24/7 read and write usage. Stopping only after the machine's power
supply died.

Most serious issues (crashes or data corruption) have been due to
[kernel
bugs](https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs). All
of which are fixed in stable releases.

## Why use FUSE? Why not a kernel based solution?

As with any solution to a problem, there are advantages and
disadvantages to each one.

A FUSE based solution has all the downsides of FUSE:

- Higher IO latency due to the trips in and out of kernel space
- Higher general overhead due to trips in and out of kernel space
- Double caching when using page caching
- Misc limitations due to FUSE's design

But FUSE also has a lot of upsides:

- Easier to offer a cross platform solution
- Easier forward and backward compatibility
- Easier updates for users
- Easier and faster release cadence
- Allows more flexibility in design and features
- Overall easier to write, secure, and maintain
- Much lower barrier to entry (getting code into the kernel takes a
  lot of time and effort initially)

FUSE was chosen because of all the advantages listed above. The
negatives of FUSE do not outweigh the positives.

## Is my OS's libfuse needed for mergerfs to work?

No. Normally `mount.fuse` is needed to get mergerfs (or any FUSE
filesystem to mount using the `mount` command but in vendoring the
libfuse library the `mount.fuse` app has been renamed to
`mount.mergerfs` meaning the filesystem type in `fstab` can simply be
`mergerfs`. That said there should be no harm in having it installed
and continuing to using `fuse.mergerfs` as the type in `/etc/fstab`.

If `mergerfs` doesn't work as a type it could be due to how the
`mount.mergerfs` tool was installed. Must be in `/sbin/` with proper
permissions.

## Why was splice support removed?

After a lot of testing over the years, splicing always appeared to
at best, provide equivalent performance, and in some cases, worse
performance. Splice is not supported on other platforms forcing a
traditional read/write fallback to be provided. The splice code was
removed to simplify the codebase.

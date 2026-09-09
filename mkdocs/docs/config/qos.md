# qos

* type: `BOOL`
* default: `false`
* example: `qos=true`

Per-client quality of service: attribute each read and write to the
process that asked for it, match it against a ruleset, and give the
worker thread that class's I/O priority, nice value and bandwidth
allowance for the duration of the request.


## Why this exists

mergerfs is one daemon serving every process that touches the pool. To
the kernel, all pool I/O is issued by one set of threads in one cgroup,
so block layer priority cannot tell a media player apart from a torrent
client -- `ionice`, systemd's `IOSchedulingClass=` and cgroup
`io.weight` are all applied to the *daemon*, not to whoever is behind
the request. Priority is flattened.

This restores the distinction inside the daemon, where the caller is
still known.

[proxy-ioprio](proxy-ioprio.md) addresses the same problem from the
other end: it copies whatever ioprio the caller happens to have. That
is simpler, but it only works if every client is correctly `ionice`d by
whoever started it -- which is awkward for programs that respawn
workers under a single service, and impossible for clients inside a
container the daemon does not control. With `qos` the policy lives in
one file and is written in terms of *who is asking* and *what they are
touching*.

The two are independent and there is no reason to enable both.


## Options

| option | default | meaning |
|---|---|---|
| `qos` | `false` | master switch |
| `qos.rules` | none | path to a rules file; assigning it loads it |
| `qos.ruleset` | - | read only: the ruleset as parsed |
| `qos.stats` | - | counters; assign `reset` to zero them |
| `qos.max-sleepers` | `-1` | threads that may sleep in a throttle at once; `-1` is half the process thread pool |
| `qos.max-sleep-ms` | `50` | longest any one request may be delayed |
| `qos.distress-ms` | `50` | service time below which a protected class is never considered to be suffering |
| `qos.distress-factor` | `3.0` | multiple of a resource's quiet latency that counts as distress |

Every one of these is readable and writable at runtime through the
[runtime interface](../runtime_interface.md):

```sh
getfattr -n user.mergerfs.qos.stats --only-values /mnt/pool/.mergerfs
setfattr -n user.mergerfs.qos.rules -v /etc/mergerfs/qos.rules /mnt/pool/.mergerfs
```

Assigning `qos.rules` is also how a ruleset is reloaded. A file that
fails to parse is rejected with the offending line number logged to
syslog, and the running ruleset is left untouched.


## The rules file

A single mount option pointing at a file, rather than a value crammed
into `/etc/fstab`, because rules contain commas and fstab does not
forgive that.

```
# Everything after # is a comment.

capacity /mnt/disk1 180M          # measured throughput of a resource
capacity default    120M          # fallback for unlisted resources

class <name> [protect] [ioprio=…] [nice=…] [rate=…] [burst=…] [yield=…] [floor=…]

match <field> <op> <pattern> [<field> <op> <pattern> …] -> <class>

default <class>
```

Rules are evaluated in order and the first whose conditions *all* match
wins. A class must be defined before a rule names it.

### Class attributes

| attribute | meaning |
|---|---|
| `ioprio=` | `rt:0`-`rt:7`, `be:0`-`be:7`, `idle`, `none` |
| `nice=` | `-20` to `19` |
| `rate=` | absolute (`8M`) or a share of the resource's capacity (`10%`) |
| `burst=` | bucket depth; defaults to one second of `rate` |
| `protect` | never throttled, and its latency drives the governor |
| `critical` | never throttled, and *not* a control signal |
| `yield=` | `0`-`100`: how hard this class gives way under pressure |
| `floor=` | never back off below this; absolute or a percentage |

### Match fields

| field | matched against |
|---|---|
| `cgroup` | `/proc/<pid>/cgroup` -- identifies the container |
| `comm` | `/proc/<pid>/comm` -- identifies the program |
| `cmdline` | `/proc/<pid>/cmdline`, arguments joined by spaces |
| `uid`, `gid` | the caller's credentials |
| `path` | the path within the pool |
| `op` | `read` or `write` |

Operators are `=` for exact match and `~` for an
[fnmatch(3)](https://man7.org/linux/man-pages/man3/fnmatch.3.html)
glob. `FNM_PATHNAME` is not set, so `path ~ /TV/*` covers everything
beneath `/TV`.

`uid`, `gid` and `op` accept only `=`.

Wrap a pattern in single or double quotes to keep spaces in it. This is
not decoration -- the names worth matching include `"Plex Transcoder"`
and `"Plex Media Scanner"`, and a command line is nothing but spaces.


### Why `cmdline` exists

Some programs do very different jobs under one name. Plex runs credits
and intro detection using the *same* `Plex Transcoder` binary that
serves playback, reading the *same* media file. Neither `comm` nor
`path` can tell those apart. The command line can: a detection job
writes into `.../Transcode/Detection/`, a playback transcode feeds a
session.

```
match comm = "Plex Transcoder" cmdline ~ */Transcode/Detection/* -> analysis
match comm = "Plex Transcoder"                                   -> playback
```

Measured on an otherwise idle pool, with `analysis` capped at 8 MiB/s:
the detection job ran at 8.1 MiB/s and the playback job, same binary and
same file, at 730 MiB/s.


### Why `critical` exists

`critical` marks a class that playback *synchronously waits on*, as
opposed to playback itself. Plex's EasyAudioEncoder is the canonical
case: transcoding EAC3, TrueHD or DTS audio runs
`Plex Transcoder -codec:1 eac3_eae`, which hands the audio to EAE and
blocks until it answers.

Such a helper looks like background work -- it is not the stream, it
does not appear in a session -- but slowing it slows the stream stalled
behind it. That is a priority inversion, and it is an easy one to
create by accident: a governor that suspends "background" processes to
protect playback will happily suspend the one service playback is
waiting for, and hang the very thing it was protecting.

A `critical` class is never delayed, whatever else the ruleset says
about it, and its latency is not used as a control signal.

```
class eae critical
match comm ~ EasyAudioEncode* -> eae
match comm = "Plex EAE Service" -> eae
```


## Rates are per resource

A resource is a branch -- in practice, a disk. Each class gets its own
token bucket per branch, so a class limited to 10% gets a tenth of
*each* disk rather than a tenth of the pool split across all of them.
Two players reading two different disks never compete for one
allowance.

Percentages need a capacity to resolve against. Measure it:

```sh
mergerfs.qos-bench /mnt/pool -o /etc/mergerfs/qos.capacity
```

and include those lines in the rules file. A percentage with no
capacity declared for its resource is left unlimited rather than
throttled against a guess.


## The adaptive governor

A fixed cap is a blunt instrument: it slows downloads even when nothing
is playing. Marking a class `protect` turns on a feedback loop instead.

mergerfs times that class's requests per resource and compares the
smoothed figure against the quietest that resource has managed. When it
is worse by `qos.distress-factor` -- and worse than `qos.distress-ms`
in absolute terms -- *and* a yielding class is active on that same
resource, pressure rises. Every yielding class's allowance is then
scaled by `1 - (pressure × yield / 100)`, bounded below by its `floor`.

When the latency recovers, or playback stops, pressure decays back to
zero and the allowances return to whatever they were configured for --
which for downloads is normally no limit at all.

The result: bulk traffic runs flat out on a disk nobody is streaming
from, gives way gradually on one that is, and never stops entirely.

Pressure rises multiplicatively and falls additively. A stutter has
already been heard by the time it is measured, so the loop backs off
hard and returns gently.

`qos.distress-ms` is the knob that matters and it is device dependent.
The `50` default suits spinning disks, where 50ms is a genuine stall.
On an SSD or NVMe pool it will never be reached and the governor will
never engage -- use single-digit milliseconds there. Setting it to `0`
removes noise suppression entirely and is not recommended.


## Example

```
capacity default 150M

# Playback is what we protect. Never throttled; its latency is the
# signal everything else is governed by.
class playback  protect ioprio=rt:0 nice=-5

# Services playback blocks on. Never throttled, never a signal.
class helpers   critical

# A media server's own background work -- scans, thumbnails, chapter
# and credits detection -- yields, but gently.
class scanning  yield=60  floor=10% ioprio=be:6 nice=10

# Downloads yield first and hardest, but never stop.
class downloads yield=100 floor=5%  ioprio=idle nice=19

# Audio helpers first: a transcode blocks on these.
match comm ~ EasyAudioEncode*                     -> helpers
match comm = "Plex EAE Service"                   -> helpers

# A media server's analysis work runs under the same binary as
# playback; only the command line separates them.
match comm = "Plex Transcoder" cmdline ~ */Transcode/Detection/* -> scanning
match comm = "Plex Media Scanner"                 -> scanning
match comm ~ ffdetect                             -> scanning

# Real transcodes and direct play, across every server.
match comm = "Plex Transcoder"                    -> playback
match comm ~ *ffmpeg*                             -> playback
match comm ~ jellyfin                             -> playback
match comm ~ EmbyServer                           -> playback

# Anything else those containers do.
match cgroup ~ *lxc/311[123]*                     -> scanning

# Downloaders.
match comm ~ qbittorrent*                         -> downloads
match comm ~ sabnzbd*                             -> downloads

default scanning
```

A client that reads the pool over NFS or SMB -- Kodi on another
machine, typically -- arrives as `nfsd` or `smbd`, not as itself.
Match those, or match on `path`, since the process behind the export is
not the one you care about.


## Testing it

A ruleset that looks right is not the same as playback that does not
stutter. `mergerfs.qos-playback-test` models a player: it reads a real
file out of the pool at a fixed bitrate through a jitter buffer, starts
and stops competing load partway through, and counts the moments the
buffer ran dry.

```sh
mergerfs.qos-playback-test /mnt/pool/Movies/film.mkv \
    --bulk-file /mnt/pool/TV/something.mkv \
    -b 25M --buffer 5 -d 60 --bulk-start 10 --bulk-stop 45 \
    -m /mnt/pool
```

It exits non-zero if the buffer ever emptied. The `pressure` column
shows the governor engaging and releasing.


## Limitations

**Buffered writes are attributed to the kernel, not the writer.** A
write that lands in the page cache is flushed later by kernel writeback
threads, which are not the calling process and carry none of its
identity. Those requests fall to the `default` class. Rate limiting a
writer works because the limit is applied when the write *enters*
mergerfs; ioprio on the worker thread does not, for the same reason it
does not for any other buffered write.

**Throttling is bounded, and therefore best effort.** mergerfs hands
requests from its read threads to a bounded process thread queue. A
sleeping process thread is one not serving anyone, and once that queue
backs up, every class queues behind it. So no more than
`qos.max-sleepers` threads sleep at once and no request is delayed
longer than `qos.max-sleep-ms`; anything that would exceed either is let
through and counted in the `passed` stat. A class generating requests
far faster than the limiter can absorb will exceed its rate. If
`passed` badly outweighs `throttled`, raise `qos.max-sleepers` --
accepting that the queue is then likelier to stall.

**Classification is cached per pid for ten seconds.** A pid recycled
onto a different class inside that window is briefly misclassified.

**Only reads and writes are governed.** Metadata operations are not,
being neither large nor slow enough to be worth the syscalls.

**Direct access to a branch is invisible.** Anything reading or writing
`/mnt/disk1` rather than the pool never reaches mergerfs and cannot be
classified. Rebalance scripts and the like need the host's own
`ionice`.


## Supported platforms

Linux only. `ioprio_set` and `/proc/<pid>/cgroup` have no equivalent
elsewhere; on other platforms the option parses and does nothing.


## Performance impact

A ruleset that cannot change anything is detected at parse time and
skipped entirely. Otherwise classification costs one small `/proc` read
per process per ten seconds, cached per worker thread, and only for the
fields some rule actually uses. Applying a class costs one
`ioprio_set` and one `setpriority` per *change*, not per request.

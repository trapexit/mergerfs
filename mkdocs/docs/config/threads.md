# threads

There are multiple thread pools used in mergerfs to provide
parallel behaviors.


## read-thread-count

Defaults to `0`

The number of threads used to read messages from the kernel. If used
alone processing will be done on the same thread. If
`process-thread-count` is enabled then this thread pool will read
messages and the process thread pool will do the work. This can
increase how much mergerfs can process at one time but may reduce
throughput.

* `read-thread-count=0` and `process-thread-count=-1`: Create `1`
  read+process thread per logical CPU core upto `8`.
* `read-thread-count=N` where `N>0` and `process-thread-count=-1`:
  Create a thread pool of `N` read+process threads.
* `read-thread-count=N` where `N<0` and `process-thread-count=-1`:
  Create a read+process thread pool of `CPUCount / -N`
  threads. Minimum of `1`.
* `read-thread-count=0` and `process-thread-count=0`: Create `2` read
  threads and a process thread per logical CPU core upto `8`.
* `read-thread-count=0` and `process-thread-count!=-1`: Creates `2`
  read threads and number of process threads as defined below.


## process-thread-count

Defaults to `-1`

When enabled this sets the number of threads in the message processing pool.

* `process-thread-count=-1`: Process thread pool is disabled.
* `process-thread-count=0`: Create `1` thread process thread per
  logical CPU core upto `8`.
* `process-thread-count=N` where `N>0`: Create a thread pool of `N` threads.
* `process-thread-count=N` where `N<-1`: Create a thread pool of `CPUCount /
  -N` threads. Minimum of 1.


## process-thread-queue-depth

Defaults to `2`

Sets the depth queue for the processing thread queue per
thread. Meaning if the read threads are getting requests faster than
can be processed they will be queued up upto the queue depth. Despite
the calculation being per thread the queue depth is shared across all
in the pool.

* `process-thread-queue-depth=N` where `N>0`: Sets the number of
  outstanding requests that the process thread pool can have to `N *
  process-thread-count`. If requests come in faster than can be
  processed and the max queue depth hit then queuing the request will
  block in order to limit memory growth.
* `process-thread-queue-depth<=0`: Sets the queue depth to 2. May be
  used in the future to set dynamically.


## fuse-read-spin-us

Defaults to `0` (disabled)

Applies only when `process-thread-count=-1` (read threads also process
requests). After handling a request, each read thread polls for the
next request for up to this many microseconds before going to sleep.

This lowers per-request latency for synchronous, single-stream workloads
(sequential reads, small writes, metadata-heavy walks such as `ls -l`,
`find`, or `rsync` scans) at the cost of CPU: while requests keep
arriving, a read thread stays busy instead of sleeping. On a quiet,
low-rate request stream this costs roughly one CPU core's worth of
polling with no benefit. An idle mount costs nothing.

`50` is the only value tested. Larger values increase CPU use. Parallel
multi-stream workloads and large (1MiB) writes showed no change.

* `fuse-read-spin-us=0`: Disabled. Read threads block immediately.
* `fuse-read-spin-us=N`: Poll for up to `N` microseconds after each
  request.

# threads

There are multiple thread pools used in mergerfs to provide
parallel behaviors.


## read-thread-count

The number of threads used to read (and possibly process) messages
from the kernel.

* `read-thread-count=0`: Create a thread pool sized to the number of
  logical CPUs.
* `read-thread-count=N` where `N>0`: Create a thread pool of `N` threads.
* `read-thread-count=N` where `N<0`: Create a thread pool of `CPUCount /
  -N` threads.
* `read-thread-count=-1` where `process-thread-count=-1`: Creates `2`
  read threads and `max(2,CPUCount-2)` process threads.
* Defaults to `0`.

When `process-thread-count=-1` (the default) this option sets the
number of threads which read and then process requests from the
kernel.

When `process-thread-count` is set to anything else mergerfs will
create two thread pools. A "read" thread pool which just reads from
the kernel and hands off requests to the "process" thread pool.

Generally, only 1 or 2 "read" threads are necessary.


## process-thread-count

When enabled this sets the number of threads in the message processing pool.

* `process-thread-count=-1`: Process thread pool is disabled.
* `process-thread-count=0`: Create a thread pool sized to the number
  of logical CPUs.
* `process-thread-count=N` where `N>0`: Create a thread pool of `N` threads.
* `process-thread-count=N` where `N<-1`: Create a thread pool of `CPUCount /
  -N` threads.
* Defaults to `-1`.


## process-thread-queue-depth

* `process-thread-queue-depth=N` where `N>0`: Sets the number of outstanding
  requests that a process thread can have to N. If requests come in
  faster than can be processed and the max queue depth hit then
  queuing the request will block in order to limit memory growth.
* `process-thread-queue-depth=0`: Sets the queue depth to the thread
  pool count.
* Defaults to `0`.

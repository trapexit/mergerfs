# Resource Usage and Management

## Usage

* threads
    * configurable number of [threads](config/threads.md)
        * reading from kernel
        * processing messages from kernel
        * readdir concurrency
* memory
    * 1MB+ pre reader thread + inflight processing for messages
      depending on [fuse-msg-size](config/fuse-msg-size.md)
    * buffers allocated temporarily for reading directories
    * [gidcache](faq/technical_behavior_and_limitations.md#how-does-mergerfs-handle-credentials)
    * FUSE nodes
    * noforget forgotten nodes


## Management

* To limit the risk of the Linux kernel's OOM Killer targeting
  mergerfs it sets its
  [oom_score_adj](https://man7.org/linux/man-pages/man5/proc_pid_oom_score_adj.5.html)
  value to -990.
* mergerfs increases [its available file descriptor and file size
  limit.](https://www.man7.org/linux/man-pages/man3/setrlimit.3p.html)
* mergerfs lowers its [scheduling
  priority](https://man7.org/linux/man-pages/man3/setpriority.3p.html)
  to -10 ([by default](config/options.md))
* The [readahead](config/readahead.md) values of mergerfs itself and
  managed filesystems can be modified.

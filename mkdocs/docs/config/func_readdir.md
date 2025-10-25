# func.readdir

* type: `STR`
* default: `seq`
* examples: `func.readdir=seq`, `func.readdir=cor:4`

`readdir` has policies to control how to read directory content.

| Policy | Description |
| ------ | ----------- |
| seq | "sequential" : Iterate sequentially over branches in the order defined in `branches`. This is the default and traditional behavior found prior to the readdir policy introduction. This will be increasingly slower as more branches are added to the pool. Especially if needing to wait for drives to spin up or network filesystems to respond. |
| cosr:N:M | "concurrent open, sequential read" : Concurrently open branch directories using a thread pool and process them in the order defined in `branches`. This keeps memory and CPU usage low while also reducing the time spent waiting on branches to respond. `N` is the number of threads. If negative it will be the core count divided by `abs(N)`. `M` is the queue depth. If either value is `0` it will be decided based on system configuration. |
| cosr:N | cosr:N:M with M = 0 |
| cosr | cosr:N:M with N = 0 and M = 0 |
| cor:N:M | "concurrent open and read" : Concurrently open branch directories and immediately start reading their contents using a thread pool. This will result in slightly higher memory and CPU usage but reduced latency. Particularly when using higher latency / slower speed network filesystem branches. Unlike `seq` and `cosr` the order of files could change due the async nature of the thread pool. This should not be a problem since the order of files returned in not guaranteed. `N` is the number of threads. If negative it will be the core count divided by `abs(N)`. `M` is the queue depth. If either value is `0` it will be decided based on system configuration. |
| cor:N | cosr:N:M with M = 0 |
| cor | cosr:N:M with N = 0 and M = 0 |

Keep in mind that `readdir` mostly just provides a list of file names
in a directory and possibly some basic metadata about said files. To
know details about the files, as one would see from commands like
`find` or `ls`, it is required to call `stat` on the file which is
controlled by `fuse.getattr`.

Until Linux v6.16, despite being able to change the size of FUSE
messages, `readdir` requests were limited to 1 page (4KiB) in size per
message. This limitation limited throughput and therefore performance
of the `readdir` call. In v6.16 that was changed to be able to grow to
the [fuse-msg-size](fuse-msg-size.md) which allows a lot more data to
be sent per request and therefore much better performance in certain
situations.


## Technical Details

* On Linux
  [getdents](https://man7.org/linux/man-pages/man2/getdents.2.html) is
  used instead of
  [readdir](https://man7.org/linux/man-pages/man2/readdir.2.html) in
  order to leverage larger buffers for better performance on larger
  directory reads as well as have more control in concurrent policies.
* [readdir](https://man7.org/linux/man-pages/man2/readdir.2.html) is
  used on FreeBSD.

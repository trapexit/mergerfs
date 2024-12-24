# func.readdir

examples: `func.readdir=seq`, `func.readdir=cor:4`

`readdir` has policies to control how it reads directory content.

| Policy | Description |
| ------ | ----------- |
| seq    | "sequential" : Iterate sequentially over branches in the order defined in `branches`. This is the default and traditional behavior found prior to the readdir policy introduction. This will be increasingly slower as more branches are added to the pool. Especially if needing to wait for drives to spin up or network filesystems to respond. |
| cosr   | "concurrent open, sequential read" : Concurrently open branch directories using a thread pool and process them in the order defined in `branches`. This keeps memory and CPU usage low while also reducing the time spent waiting on branches to respond. Number of threads defaults to the number of logical cores. Can be overwritten via the syntax `func.readdir=cosr:N` where `N` is the number of threads. |
| cor    | "concurrent open and read" : Concurrently open branch directories and immediately start reading their contents using a thread pool. This will result in slightly higher memory and CPU usage but reduced latency. Particularly when using higher latency / slower speed network filesystem branches. Unlike `seq` and `cosr` the order of files could change due the async nature of the thread pool. This should not be a problem since the order of files listed in not guaranteed. Number of threads defaults to the number of logical cores. Can be overwritten via the syntax `func.readdir=cor:N` where `N` is the number of threads. |

Keep in mind that `readdir` mostly just provides a list of file names
in a directory and possibly some basic metadata about said files. To
know details about the files, as one would see from commands like
`find` or `ls`, it is required to call `stat` on the file which is
controlled by `fuse.getattr`.

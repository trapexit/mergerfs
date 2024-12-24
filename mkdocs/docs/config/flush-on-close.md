# flush-on-close

By default, FUSE would issue a flush before the release of a file
descriptor. This was considered a bit aggressive and a feature added
to give the FUSE server the ability to choose when that happens.

* `flush-on-close=always`
* `flush-on-close=never`
* `flush-on-close=opened-for-write`
* Defaults to `opened-for-write`.

For now it defaults to `opened-for-write` which is less aggressive
than the behavior before this feature was added. It should not be a
problem because the flush is really only relevant when a file is
written to. Given flush is irrelevant for many filesystems in the
future a branch specific flag may be added so only files opened on a
specific branch would be flushed on close.

## References

* [https://lkml.kernel.org/linux-fsdevel/20211024132607.1636952-1-amir73il@gmail.com/T/](https://lkml.kernel.org/linux-fsdevel/20211024132607.1636952-1-amir73il@gmail.com/T/)

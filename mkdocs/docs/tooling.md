# Tooling

## preload.so

EXPERIMENTAL

For some time there has been work to enable passthrough IO in
FUSE. Passthrough IO would allow for near native performance with
regards to reads and writes (at the expense of certain mergerfs
features.) In Linux v6.9 that feature made its way into the kernel
however in a somewhat limited form which is incompatible with aspects
of how mergerfs currently functions. While work will continue to
support passthrough IO in mergerfs this library was created to offer
similar functionality in a more limited way.

`/usr/lib/mergerfs/preload.so`

This [preloadable
library](https://man7.org/linux/man-pages/man8/ld.so.8.html#ENVIRONMENT)
overrides the creation and opening of files in order to simulate
passthrough file IO. It catches the open/creat/fopen calls, has
mergerfs do the call, queries mergerfs for the branch the file exists
on, reopens the file on the underlying filesystem and returns that
instead. Meaning that you will get native read/write performance
because mergerfs is no longer part of the workflow. Keep in mind that
this also means certain mergerfs features that work by interrupting
the read/write workflow, such as `moveonenospc`, will no longer work.

Also, understand that this will only work on dynamically linked
software. Anything statically compiled will not work. Many GoLang and
Rust apps are statically compiled.

The library will not interfere with non-mergerfs filesystems. The
library is written to always fallback to returning the mergerfs opened
file on error.

While the library was written to account for a number of edgecases
there could be some yet accounted for so please report any oddities.

Thank you to
[nohajc](https://github.com/nohajc/mergerfs-io-passthrough) for
prototyping the idea.

### casual usage

```sh
LD_PRELOAD=/usr/lib/mergerfs/preload.so touch /mnt/mergerfs/filename
```

Or run `export LD_PRELOAD=/usr/lib/mergerfs/preload.so` in your shell
or place it in your shell config file to have it be picked up by all
software ran from your shell.


### Docker and Podman usage

Assume `/mnt/fs0` and `/mnt/fs1` are pooled with mergerfs at `/media`.

All mergerfs branch paths _must_ be bind mounted into the container at
the same path as found on the host so the preload library can see
them.

**NOTE:** Since a container can have its own OS setup there is no
guarentee that `preload.so` from the host install will be compatible
with the loader found in the container. If that is true it simply
won't work and shouldn't cause any issues.


```sh
docker run \
  -e LD_PRELOAD=/usr/lib/mergerfs/preload.so \
  -v /usr/lib/mergerfs/preload.so:/usr/lib/mergerfs/preload.so:ro \
  -v /media:/media \
  -v /mnt:/mnt \
  ubuntu:latest \
  bash
```

or more explicitly

```sh
docker run \
  -e LD_PRELOAD=/usr/lib/mergerfs/preload.so \
  -v /usr/lib/mergerfs/preload.so:/usr/lib/mergerfs/preload.so:ro \
  -v /media:/media \
  -v /mnt/fs0:/mnt/fs0 \
  -v /mnt/fs1:/mnt/fs1 \
  ubuntu:latest \
  bash
```


### systemd unit

Use the `Environment` option to set the LD_PRELOAD variable.

* [https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html#Command%20lines](https://www.freedesktop.org/software/systemd/man/latest/systemd.service.html#Command%20lines)
* [https://serverfault.com/questions/413397/how-to-set-environment-variable-in-systemd-service](https://serverfault.com/questions/413397/how-to-set-environment-variable-in-systemd-service)

```
[Service]
Environment=LD_PRELOAD=/usr/lib/mergerfs/preload.so
```

## Misc

- https://github.com/trapexit/mergerfs-tools
  - mergerfs.ctl: A tool to make it easier to query and configure mergerfs at runtime
  - mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
  - mergerfs.dedup: Will help identify and optionally remove duplicate files
  - mergerfs.dup: Ensure there are at least N copies of a file across the pool
  - mergerfs.balance: Rebalance files across filesystems by moving them from the most filled to the least filled
  - mergerfs.consolidate: move files within a single mergerfs directory to the filesystem with most free space
- https://github.com/trapexit/scorch
  - scorch: A tool to help discover silent corruption of files and keep track of files
- https://github.com/trapexit/bbf
  - bbf (bad block finder): a tool to scan for and 'fix' hard drive bad blocks and find the files using those blocks

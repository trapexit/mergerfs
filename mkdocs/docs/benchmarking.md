# Benchmarking

Filesystems are complicated. They do many things and many of those are
interconnected. Additionally, the OS, drivers, hardware, etc. can all
impact performance. Therefore, when benchmarking, it is **necessary**
that the test focuses as narrowly as possible.

For most throughput is the key benchmark. To test throughput `dd` is
useful but **must** be used with the correct settings in order to
ensure the filesystem or device is actually being tested. The OS can
and will cache data. Without forcing synchronous reads and writes
and/or disabling caching the values returned will not be
representative of the device's true performance.

When benchmarking through mergerfs ensure you only use 1 branch to
remove any possibility of the policies or differences in underlying
filesystems complicating the situation. Benchmark the underlying
filesystem first and then mount mergerfs with that same filesystem and
that. Throughput **will be** lower but if you are experiencing speeds
below your expectation you will need to narrow down precisely which
component is leading to the slowdown. Preferably test the following in
the order listed (but not combined).

1. Enable `nullrw` mode with `nullrw=true`. This will effectively make
   reads and writes no-ops. Removing the underlying device /
   filesystem from the equation. This will give us the top theoretical
   speeds.
2. Configure mergerfs to use a `tmpfs` branch. `tmpfs` is a RAM
   disk. Extremely high speed and very low latency. This is a more
   realistic best case scenario. Example: `mount -t tmpfs -o size=2G
   tmpfs /tmp/tmpfs`
3. Configure mergerfs to use a local device filesystem branch. NVMe,
   SSD, HDD, etc. Test them individually. If you have different
   interconnects / controllers use the same storage device when
   testing to ensure consistency.
4. Configure mergerfs to use any network filesystems you plan to use
   one at a time. It may also be worth trying a different network
   filesystem. `NFS` vs `CIFS/SMB/Samba` vs `sshfs`, etc.

Once you find the component which has the performance issue you can do
further testing with different options to see if they impact
performance. If you find a specific system, device, filesystem,
controller, etc. that performs poorly contact the author so it can be
investigated further.

Sometimes the problem is really the application accessing or writing
data through mergerfs. Some software use small buffer sizes which can
lead to more requests and therefore greater overhead. You can test
this out yourself by replacing `bs=1M` in the examples below with
`ibs` or `obs` and using a size of `512` instead of `1M`. In one
example test using `nullrw` the write speed dropped from 4.9GB/s to
69.7MB/s when moving from `1M` to `512`. Similar results were had when
testing reads. Small writes overhead may be improved by leveraging a
write cache but in casual tests little gain was found. If you have an
app that appears slow with mergerfs it could be due to this. `strace`
can be used with the app in question or mergerfs to see the size of
read/writes. Contact the software author or worse case the mergerfs
author so it may be investigated further.


### write benchmark

```
$ dd if=/dev/zero of=/mnt/mergerfs/1GB.file bs=1M count=1024 oflag=nocache conv=fdatasync status=progress
```

### read benchmark

```
$ dd if=/mnt/mergerfs/1GB.file of=/dev/null bs=1M count=1024 iflag=nocache conv=fdatasync status=progress
```

### other benchmarks

If you are attempting to benchmark other behaviors you must ensure you
clear kernel caches before runs. In fact it would be a good deal to
run before the read and write benchmarks as well just in case.

```
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches
```

## Additional Reading

* [Tweaking Performance](performance.md)
* [Options](config/options.md)
* [Tips and Notes](tips_notes.md)

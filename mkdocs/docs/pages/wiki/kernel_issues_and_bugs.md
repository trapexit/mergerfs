There have been a number of kernel issues / bugs over the years which mergerfs has run into. Here is a list of them for reference and posterity.

## NFS and EIO errors

https://lore.kernel.org/linux-fsdevel/20240228160213.1988854-1-mszeredi@redhat.com/T/

Over the years some users have reported that while exporting mergerfs via NFS, after significant filesystem activity, not only will the NFS client start returning ESTALE and EIO errors but mergerfs itself would start returning EIO errors. The problem was that no one could reliability reproduce the issue. After a string of reports in late 2023 and early 2024 more investigation was done.

In Linux 5.14 new validation was put into FUSE which caught a few invalid situations and would tag a FUSE node as invalid if a check failed. Such checks include invalid file type, changing of type from one request to another, a size greater than 63bit, and the generation of a inode changing while in use.

What happened was that mergerfs was using a different fixed, non-zero value for the generation of all nodes as it was suggested that unique inode + generation pairs are needed for proper integration with NFS. That non-zero value was being sent back to the kernel when a lookup request was made for root. The reason this was hard to track down was because NFS almost uniquely uses an API which can lead to a lookup of the root node that simply won't happen under normal workloads and usage. And that lookup will only happen if child nodes of the root were forgotten but NFS still had a handle to that node and later asked for details about it. It would trigger a set of requests to lookup info on those nodes.

This wasn't a bug in FUSE but mergerfs. However, the incorrect behavior of mergerfs lead to FUSE behave in an unexpected and incorrect manner. It would issue a lookup of the "parent of a child of the root" and mergerfs would send the invalid generation value. As a result the kernel would mark the root node as "bad" which would then trigger the kernel to issue a "forget root" message. In between those it would issue a request for the parent of the root... which doesn't exist.

So the kernel was doing two invalid things. Requesting the parent of the root and then when that failed issuing a forget for the root. These led to chasing after the wrong possible causes.

The proposed change is for FUSE to revert the marking of root node bad if the generation is non-zero and warn about it. It will mark the node bad but not unhash/forget/remove it.

mergerfs in v2.40.1 ensures that generation for root is always 0 on lookup which should work across any kernel version.

## Truncated files

This was a bug with mmap and FUSE on 32bit platforms. Should be fixed in all LTS releases.

- https://marc.info/?l=linux-fsdevel&m=155550785230874&w=2

## Crashing on OpenVZ

There is/was a bug in the OpenVZ kernel with regard to how it handles ioctl calls. It was making invalid requests which would lead to crashes due to mergerfs not expecting them.

- https://bugs.openvz.org/browse/OVZ-7145
- https://www.mail-archive.com/devel@openvz.org/msg37096.html

## Really bad mmap performance

There is/was a bug in caching which affects overall performance of mmap through FUSE in Linux 4.x kernels. It is fixed in 4.4.10 and 4.5.4.

- https://lkml.org/lkml/2016/3/16/260
- https://lkml.org/lkml/2016/5/11/59

## Heavy load and memory pressure leads to kernel panic

- https://lkml.org/lkml/2016/9/14/527
- https://lkml.org/lkml/2016/10/4/1
- https://www.theregister.com/2016/10/05/linus_torvalds_admits_buggy_crap_made_it_into_linux_48/

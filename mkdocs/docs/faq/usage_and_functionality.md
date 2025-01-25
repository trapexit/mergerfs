# Usage and Functionality

## Can mergerfs be used with filesystems which already have data?

Yes. mergerfs is really just a proxy and does **NOT** interfere with
the normal form or function of the filesystems, mounts, or paths it
manages. It literally is interacting with your filesystems as any
other application does. It can not do anything that any other random
piece of software can't do.

mergerfs is **not** a traditional filesystem that takes control over
the underlying disk or block device. mergerfs is **not** RAID. It does
**not** manipulate the data that passes through it. It does **not**
shard data across filesystems. It only shards some **behavior** and
aggregates others.


## Can filesystems be removed from the pool without affecting them?

Yes. See previous question's answer.


## Can mergerfs be removed without affecting the data?

Yes. See the previous question's answer.


## Can filesystems be moved to another pool?

Yes. See the previous question's answer.


## Can filesystems be part of multiple pools?

Yes.


## How do I migrate data into or out of the pool when adding/removing filesystems?

There is no need to do so. See the previous questions.


## How do I remove a filesystem but keep the data in the pool?

Nothing special needs to be done. Remove the branch from mergerfs'
config and copy (rsync) the data from the removed filesystem into the
pool. The same as if it were you transfering data from one filesystem
to another.

If you wish to continue using the pool with all data available while
performing the transfer simply create a temporary pool without the
branch in question and then copy the data from the branch to the
temporary pool. It would probably be a good idea to set the branch
mode to `RO` prior to doing this to ensure no new content is written
to the filesystem while performing the copy. However, it is typically
good practice to run rsync or rclone again after the first copy
finishes to ensure nothing is left behind.

NOTE: Above recommends to "copy" rather than "move" because you want
to ensure that your data is transfered before wiping the drive or
filesystem.


## Can filesystems still be used directly? Outside of mergerfs while pooled?

Yes, out-of-band interaction is generally fine. Remember that mergerfs
is just a userland application like any other software so its
interactions with the underlying filesystems is no different. It would
be like two normal applications interacting with the
filesystem. However, it's not recommended to write to the same file
from within the pool and from without at the same time. Especially if
using page caching (`cache.files!=off`) or writeback caching
(`cache.writeback=true`). That said this risk is really not
really different from the risk of two applications writing to
the same file under normal conditions.

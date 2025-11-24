# Usage and Functionality

## What should mergerfs NOT be used for?

* Situations where you need large amounts of contiguous space beyond
  that available on any singular device. Such as putting 10GiB a file
  on 2 6GiB filesystems.
* databases: Even if the database stored data in separate files
  (mergerfs wouldn't offer much otherwise) the higher latency of the
  indirection will harm performance. Though if it is a sqlite3
  database then its likely fine.
* VM images: For the same reasons as databases. VM images are accessed
  very aggressively and mergerfs will introduce a lot of extra latency.
* As replacement for RAID: mergerfs is just for pooling branches. If
  you need device performance aggregation or high availability you
  should stick with RAID. However, it is fine to put a filesystem
  which is on a RAID setup in mergerfs.

**However, if using [passthrough](../config/passthrough.md) the
performance related issues above are less likely to be a concern. Best
to do testing for your specific use case.**


## What happens when file paths overlap?

It depends on the situation and the configuration of mergerfs. The
user chooses the algorithm (policy) on how to behave when a particular
filesystem function is requested. Which file to select or act on.

See how [policies](../config/functions_categories_policies.md) work.


## Can mergerfs be used with filesystems which already have data?

Yes. mergerfs is really just a proxy and does **NOT** interfere with
the normal form or function of the filesystems, mounts, or paths it
manages. It literally is interacting with your filesystems as any
other application does. It can not do anything that any other random
piece of software can't do.

mergerfs is **not** a traditional filesystem that takes control over
the underlying drive, block device, or partition. mergerfs is **not**
RAID. It does **not** manipulate the data that passes through it. It
does **not** shard data across filesystems. It only shards some
**behavior** and aggregates others.


## Can mergerfs be removed without affecting the data?

Yes. See the previous question's answer.


## Can filesystems be removed from the pool without affecting them?

Yes. See previous question's answer.

This is true for planned removal by unmounting mergerfs and changing
the config, changes made to mergerfs at
[runtime](../runtime_interface.md), umounting of the branch's
filesystem on the fly (whether on purpose or due to error), etc.


## What happens if a filesystem disappears at runtime?

By "disappear" meaning explicitly unmounted or due to an error the OS
removes it.

Nothing explicitly happens. mergerfs works on paths, not mounts. If
the branch path still exists mergerfs will treat it the same as it did
before. It just may not have any data there. If the branch no longer
exists it will be ignored. If the OS returns errors then mergerfs will
return those errors where appropriate. See [Error Handling and
Logging](../error_handling_and_logging.md).


## Can filesystems be moved to another pool?

Yes.


## Can filesystems be part of multiple pools?

Yes.


## How do I migrate data into or out of the pool when adding/removing filesystems?

There is no need to do so. mergerfs is a union filesystem. It works
**on top** of existing filesystems. It does **not** replace them. See
the previous questions and answers.


## How do I remove a filesystem but keep the data in the pool?

Nothing special needs to be done. Remove the branch from mergerfs'
config and copy (rsync) the data from the removed filesystem into the
pool. The same as if it were you transferring data from one filesystem
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
to ensure that your data is transferred before wiping the drive or
filesystem.


## Can filesystems still be used directly? Outside of mergerfs while pooled?

Yes, [out-of-band](https://en.wikipedia.org/wiki/Out-of-band)
interaction is generally fine. Remember that mergerfs is a userland
application so any interaction with the underlying filesystem is the
same as multiple normal applications interacting. However, it is not
recommended to write to the same file from within the pool and from
without at the same time. Especially if using page caching
(`cache.files!=off`) or writeback caching
(`cache.writeback=true`). That said this risk is really not different
from the risk of two applications writing to the same file under
normal conditions.

Keep in mind that if out-of-band changes are made while also
leveraging any caching of the filesystem layout such as with
`cache.entry`, `cache.negative-entry`, `cache.attr`, `cache.symlinks`,
or `cache.readdir` you may experience temporary inconsistency till the
cache expires. mergerfs is not actively watching all branches for
changes and the kernel will have no way to know anything changed so as
to clear or ignore the cache. This is the same issue you can have with
[remote filesystems](../remote_filesystems.md).

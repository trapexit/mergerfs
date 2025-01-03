# Usage and Functionality

## Can mergerfs be used with filesystems which already have data / are in use?

Yes. mergerfs is really just a proxy and does **NOT** interfere with
the normal form or function of the filesystems, mounts, paths it
manages. A userland application that is acting as a
man-in-the-middle. It can't do anything that any other random piece of
software can't do.

mergerfs is **not** a traditional filesystem that takes control over
the underlying block device. mergerfs is **not** RAID. It does **not**
manipulate the data that passes through it. It does **not** shard data
across filesystems. It merely shards some **behavior** and aggregates
others.


## Can filesystems be removed from the pool without affecting them?

Yes. See previous question's answer.


## Can mergerfs be removed without affecting the data?

Yes. See the previous question's answer.


## Can filesystems be moved to another pool?

Yes. See the previous question's answer.


## Can filesystems be part of multiple pools?

Yes.


## How do I migrate data into or out of the pool when adding/removing filesystems?

You don't need to. See the previous question's answer.


## How do I remove a filesystem but keep the data in the pool?

Nothing special needs to be done. Remove the branch from mergerfs'
config and copy (rsync) the data from the removed filesystem into the
pool. The same as if it were you transfering data from one filesystem
to another.

If you wish to continue using the pool while performing the transfer
simply create a temporary pool without the filesystem in question and
then copy the data. It would probably be a good idea to set the branch
to `RO` prior to doing this to ensure no new content is written to the
filesystem while performing the copy.


## Can filesystems be written to directly? Outside of mergerfs while pooled?

Yes, however, it's not recommended to use the same file from within the
pool and from without at the same time (particularly
writing). Especially if using caching of any kind (cache.files,
cache.entry, cache.attr, cache.negative_entry, cache.symlinks,
cache.readdir, etc.) as there could be a conflict between cached data
and not.

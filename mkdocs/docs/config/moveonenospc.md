# moveonenospc

When writing to a file a [number of
errors](https://man7.org/linux/man-pages/man2/write.2.html#ERRORS) are
possible. The `ENOSPC` error indicates their is no room for the
data. That could be true due to the filesystem having actually no
available space for data, or because a secondary resource (such as
inodes) has been used up, or the filesystem might have a quota feature
that is limiting how much storage a particular user may use, or (as on
`ext4` there might be a feature which reserves space for privileged
processes.

Since mergerfs [does not offer](../index.md#non-features) splitting of
files across filesystems there are situations where a file is opened
or created on a filesystem which is nearly full and eventually
receives a `ENOSPC` error despite the pool having capacity. The
`moveonenospc` feature allows the user to have some control over this
situation.

When enabled and a `write` fails with `ENOSPC` or `EDQUOT` mergerfs
will:

1. Check that the file's path is absent on every branch other than the
   source and its aliases, including read-only and no-create branches.
   If the path already exists (even as a dangling symlink), or a branch
   cannot be checked, refuse the move: removing the source could make a
   later open find a different file. Otherwise, use the `moveonenospc`
   policy to choose a destination from the eligible branches.
2. ["Move"](../faq/technical_behavior_and_limitations.md#how-does-mergerfs-handle-moving-and-copying-of-files)
   the file from the source branch to the target branch. If another entry
   appears at the destination during the copy, the move fails without
   overwriting that entry or removing the source.
3. Retry only the unwritten portion of the request, so a partially
   completed append is not duplicated.

If at any point something fails the progress so far will be cleaned up
as appropriate and the original error returned. If part of the write
already reached the file the bytes written are returned instead, as a
short write, as with a local filesystem.

If `moveonenospc` is disabled the underlying error will be returned,
or the bytes written if part of the write succeeded.

NOTE: This feature has NO effect on policies. It ONLY applies to the
literal write function. If the create function returns `ENOSPC` or the
[policy returns `ENOSPC`](functions_categories_policies.md#filtering)
that error will be returned back to the application making the
`create` request.


## Additional Reading

* [Functions, Categories, Policies](functions_categories_policies.md)
* [Why do I get an "out of space" / "no space left on device" / ENOSPC
  error even though there appears to be lots of space
  available?](../faq/why_isnt_it_working.md#why-do-i-get-an-out-of-space-no-space-left-on-device-enospc-error-even-though-there-appears-to-be-lots-of-space-available)


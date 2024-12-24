# rename and link

**NOTE:** If you're receiving errors from software when files are
moved / renamed / linked then you should consider changing the create
policy to one which is **not** path preserving, enabling
`ignorepponrename`, or contacting the author of the offending software
and requesting that `EXDEV` (cross device / improper link) be properly
handled.

`rename` and `link` are arguably the most complicated functions to
create in a union filesystem. `rename` only works within a single
filesystem or device. If a rename can't be done due to the source and
destination paths existing on different mount points it will return
**-1** with **errno = EXDEV** (cross device / improper link). So if a
`rename`'s source and target are on different filesystems within the
pool it creates an issue.

Originally mergerfs would return EXDEV whenever a rename was requested
which was cross directory in any way. This made the code simple and
was technically compliant with POSIX requirements. However, many
applications fail to handle EXDEV at all and treat it as a normal
error or otherwise handle it poorly. Such apps include: gvfsd-fuse
v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+,
NZBGet, Samba's recycling bin feature.

As a result a compromise was made in order to get most software to
work while still obeying mergerfs' policies. Below is the basic logic.

* If using a **create** policy which tries to preserve directory paths (epff,eplfs,eplus,epmfs)
  * Using the **rename** policy get the list of files to rename
  * For each file attempt rename:
    * If failure with ENOENT (no such file or directory) run **create** policy
    * If create policy returns the same branch as currently evaluating then clone the path
    * Re-attempt rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all branches with no source file
    * Remove the source from all branches which failed to rename
* If using a **create** policy which does **not** try to preserve directory paths
  * Using the **rename** policy get the list of files to rename
  * Using the **getattr** policy get the target path
  * For each file attempt rename:
    * If the source branch != target branch:
      * Clone target path from target branch to source branch
    * Rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all branches with no source file
    * Remove the source from all branches which failed to rename

The removals are subject to normal entitlement checks. If the unlink
fails it will fail silently.

The above behavior will help minimize the likelihood of EXDEV being
returned but it will still be possible.

**link** uses the same strategy but without the removals.

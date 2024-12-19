# FUNCTIONS, CATEGORIES and POLICIES

The POSIX filesystem API is made up of a number of
functions. **creat**, **stat**, **chown**, etc. For ease of
configuration in mergerfs, most of the core functions are grouped into
3 categories: **action**, **create**, and **search**. These functions
and categories can be assigned a policy which dictates which branch is
chosen when performing that function.

Some functions, listed in the category `N/A` below, can not be
assigned the normal policies. These functions work with file handles,
rather than file paths, which were created by `open` or `create`. That
said many times the current FUSE kernel driver will not always provide
the file handle when a client calls `fgetattr`, `fchown`, `fchmod`,
`futimens`, `ftruncate`, etc. This means it will call the regular,
path based, versions. `statfs`'s behavior can be modified via other
options.

When using policies which are based on a branch's available space the
base path provided is used. Not the full path to the file in
question. Meaning that mounts in the branch won't be considered in the
space calculations. The reason is that it doesn't really work for
non-path preserving policies and can lead to non-obvious behaviors.

NOTE: While any policy can be assigned to a function or category,
some may not be very useful in practice. For instance: **rand**
(random) may be useful for file creation (create) but could lead to
very odd behavior if used for `chmod` if there were more than one copy
of the file.

### Functions and their Category classifications

| Category | FUSE Functions                                                                                                                         |
| -------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens                                                    |
| create   | create, mkdir, mknod, symlink                                                                                                          |
| search   | access, getattr, getxattr, ioctl (directories), listxattr, open, readlink                                                              |
| N/A      | fchmod, fchown, futimens, ftruncate, fallocate, fgetattr, fsync, ioctl (files), read, readdir, release, statfs, write, copy_file_range |

In cases where something may be searched for (such as a path to clone)
**getattr** will usually be used.

### Policies

A policy is the algorithm used to choose a branch or branches for a
function to work on or generally how the function behaves.

Any function in the `create` category will clone the relative path if
needed. Some other functions (`rename`,`link`,`ioctl`) have special
requirements or behaviors which you can read more about below.

#### Filtering

Most policies basically search branches and create a list of files / paths
for functions to work on. The policy is responsible for filtering and
sorting the branches. Filters include **minfreespace**, whether or not
a branch is mounted read-only, and the branch tagging
(RO,NC,RW). These filters are applied across most policies.

- No **search** function policies filter.
- All **action** function policies filter out branches which are
  mounted **read-only** or tagged as **RO (read-only)**.
- All **create** function policies filter out branches which are
  mounted **read-only**, tagged **RO (read-only)** or **NC (no
  create)**, or has available space less than `minfreespace`.

Policies may have their own additional filtering such as those that
require existing paths to be present.

If all branches are filtered an error will be returned. Typically
**EROFS** (read-only filesystem) or **ENOSPC** (no space left on
device) depending on the most recent reason for filtering a
branch. **ENOENT** will be returned if no eligible branch is found.

If **create**, **mkdir**, **mknod**, or **symlink** fail with `EROFS`
or other fundamental errors then mergerfs will mark any branch found
to be read-only as such (IE will set the mode `RO`) and will rerun the
policy and try again. This is mostly for `ext4` filesystems that can
suddenly become read-only when it encounters an error.

#### Path Preservation

Policies, as described below, are of two basic classifications. `path
preserving` and `non-path preserving`.

All policies which start with `ep` (**epff**, **eplfs**, **eplus**,
**epmfs**, **eprand**) are `path preserving`. `ep` stands for
`existing path`.

A path preserving policy will only consider branches where the relative
path being accessed already exists.

When using non-path preserving policies paths will be cloned to target
branches as necessary.

With the `msp` or `most shared path` policies they are defined as
`path preserving` for the purpose of controlling `link` and `rename`'s
behaviors since `ignorepponrename` is available to disable that
behavior.

#### Policy descriptions

A policy's behavior differs, as mentioned above, based on the function
it is used with. Sometimes it really might not make sense to even
offer certain policies because they are literally the same as others
but it makes things a bit more uniform.

| Policy                                                          | Description                                                                                                                                                                     |
| --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| all                                                             | Search: For **mkdir**, **mknod**, and **symlink** it will apply to all branches. **create** works like **ff**.                                                                  |
| epall (existing path, all)                                      | For **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **epff** (but more expensive because it doesn't stop after finding a valid branch). |
| epff (existing path, first found)                               | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found where the relative path exists.                                  |
| eplfs (existing path, least free space)                         | Of all the branches on which the relative path exists choose the branch with the least free space.                                                                              |
| eplus (existing path, least used space)                         | Of all the branches on which the relative path exists choose the branch with the least used space.                                                                              |
| epmfs (existing path, most free space)                          | Of all the branches on which the relative path exists choose the branch with the most free space.                                                                               |
| eppfrd (existing path, percentage free random distribution)     | Like **pfrd** but limited to existing paths.                                                                                                                                    |
| eprand (existing path, random)                                  | Calls **epall** and then randomizes. Returns 1.                                                                                                                                 |
| ff (first found)                                                | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found.                                                                 |
| lfs (least free space)                                          | Pick the branch with the least available free space.                                                                                                                            |
| lus (least used space)                                          | Pick the branch with the least used space.                                                                                                                                      |
| mfs (most free space)                                           | Pick the branch with the most available free space.                                                                                                                             |
| msplfs (most shared path, least free space)                     | Like **eplfs** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| msplus (most shared path, least used space)                     | Like **eplus** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| mspmfs (most shared path, most free space)                      | Like **epmfs** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| msppfrd (most shared path, percentage free random distribution) | Like **eppfrd** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                          |
| newest                                                          | Pick the file / directory with the largest mtime.                                                                                                                               |
| pfrd (percentage free random distribution)                      | Chooses a branch at random with the likelihood of selection based on a branch's available space relative to the total.                                                          |
| rand (random)                                                   | Calls **all** and then randomizes. Returns 1 branch.                                                                                                                            |

**NOTE:** If you are using an underlying filesystem that reserves
blocks such as ext2, ext3, or ext4 be aware that mergerfs respects the
reservation by using `f_bavail` (number of free blocks for
unprivileged users) rather than `f_bfree` (number of free blocks) in
policy calculations. **df** does NOT use `f_bavail`, it uses
`f_bfree`, so direct comparisons between **df** output and mergerfs'
policies is not appropriate.

#### Defaults

| Category | Policy |
| -------- | ------ |
| action   | epall  |
| create   | epmfs  |
| search   | ff     |

#### func.readdir

examples: `func.readdir=seq`, `func.readdir=cor:4`

`readdir` has policies to control how it manages reading directory
content.

| Policy | Description                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 |
| ------ | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| seq    | "sequential" : Iterate over branches in the order defined. This is the default and traditional behavior found prior to the readdir policy introduction.                                                                                                                                                                                                                                                                                                                                                                                                     |
| cosr   | "concurrent open, sequential read" : Concurrently open branch directories using a thread pool and process them in order of definition. This keeps memory and CPU usage low while also reducing the time spent waiting on branches to respond. Number of threads defaults to the number of logical cores. Can be overwritten via the syntax `func.readdir=cosr:N` where `N` is the number of threads.                                                                                                                                                        |
| cor    | "concurrent open and read" : Concurrently open branch directories and immediately start reading their contents using a thread pool. This will result in slightly higher memory and CPU usage but reduced latency. Particularly when using higher latency / slower speed network filesystem branches. Unlike `seq` and `cosr` the order of files could change due the async nature of the thread pool. Number of threads defaults to the number of logical cores. Can be overwritten via the syntax `func.readdir=cor:N` where `N` is the number of threads. |

Keep in mind that `readdir` mostly just provides a list of file names
in a directory and possibly some basic metadata about said files. To
know details about the files, as one would see from commands like
`find` or `ls`, it is required to call `stat` on the file which is
controlled by `fuse.getattr`.

#### ioctl

When `ioctl` is used with an open file then it will use the file
handle which was created at the original `open` call. However, when
using `ioctl` with a directory mergerfs will use the `open` policy to
find the directory to act on.

#### rename and link

**NOTE:** If you're receiving errors from software when files are
moved / renamed / linked then you should consider changing the create
policy to one which is **not** path preserving, enabling
`ignorepponrename`, or contacting the author of the offending software
and requesting that `EXDEV` (cross device / improper link) be properly
handled.

`rename` and `link` are tricky functions in a union
filesystem. `rename` only works within a single filesystem or
device. If a rename can't be done atomically due to the source and
destination paths existing on different mount points it will return
**-1** with **errno = EXDEV** (cross device / improper link). So if a
`rename`'s source and target are on different filesystems within the pool
it creates an issue.

Originally mergerfs would return EXDEV whenever a rename was requested
which was cross directory in any way. This made the code simple and
was technically compliant with POSIX requirements. However, many
applications fail to handle EXDEV at all and treat it as a normal
error or otherwise handle it poorly. Such apps include: gvfsd-fuse
v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+,
NZBGet, Samba's recycling bin feature.

As a result a compromise was made in order to get most software to
work while still obeying mergerfs' policies. Below is the basic logic.

- If using a **create** policy which tries to preserve directory paths (epff,eplfs,eplus,epmfs)
  - Using the **rename** policy get the list of files to rename
  - For each file attempt rename:
    - If failure with ENOENT (no such file or directory) run **create** policy
    - If create policy returns the same branch as currently evaluating then clone the path
    - Re-attempt rename
  - If **any** of the renames succeed the higher level rename is considered a success
  - If **no** renames succeed the first error encountered will be returned
  - On success:
    - Remove the target from all branches with no source file
    - Remove the source from all branches which failed to rename
- If using a **create** policy which does **not** try to preserve directory paths
  - Using the **rename** policy get the list of files to rename
  - Using the **getattr** policy get the target path
  - For each file attempt rename:
    - If the source branch != target branch:
      - Clone target path from target branch to source branch
    - Rename
  - If **any** of the renames succeed the higher level rename is considered a success
  - If **no** renames succeed the first error encountered will be returned
  - On success:
    - Remove the target from all branches with no source file
    - Remove the source from all branches which failed to rename

The removals are subject to normal entitlement checks.

The above behavior will help minimize the likelihood of EXDEV being
returned but it will still be possible.

**link** uses the same strategy but without the removals.

#### statfs / statvfs

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source
filesystems based on the fragment size and sums the number of adjusted
blocks and inodes. This means you will see the combined space of all
sources. Total, used, and free. The sources however are dedupped based
on the filesystem so multiple sources on the same drive will not result in
double counting its space. Other filesystems mounted further down the tree
of the branch will not be included when checking the mount's stats.

The options `statfs` and `statfs_ignore` can be used to modify
`statfs` behavior.

#### flush-on-close

https://lkml.kernel.org/linux-fsdevel/20211024132607.1636952-1-amir73il@gmail.com/T/

By default, FUSE would issue a flush before the release of a file
descriptor. This was considered a bit aggressive and a feature added
to give the FUSE server the ability to choose when that happens.

Options:

- always
- never
- opened-for-write

For now it defaults to "opened-for-write" which is less aggressive
than the behavior before this feature was added. It should not be a
problem because the flush is really only relevant when a file is
written to. Given flush is irrelevant for many filesystems in the
future a branch specific flag may be added so only files opened on a
specific branch would be flushed on close.

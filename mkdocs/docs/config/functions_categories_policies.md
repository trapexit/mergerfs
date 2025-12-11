# functions, categories and policies

The POSIX filesystem API is made up of a number of functions. `creat`,
`stat`, `chown`, etc. For ease of configuration in mergerfs, most of
the core functions are grouped into 3 categories: `action`, `create`,
and `search`. These functions and categories can be assigned a policy
which dictates which branch is chosen when performing that
function. IE. When an application requests that a file or directory is
created how should mergerfs decide which branch to choose to attempt
the create? That is the policy.

Some functions, listed in the category `N/A` below, can not be
assigned the normal policies because they are directly related to a
file which has already been opened.


## Functions and their Category classifications

| Category | Functions |
| -------- | --------- |
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink |
| search   | access, getattr, getxattr, ioctl (directories), listxattr, open, readlink |
| N/A      | fchmod, fchown, futimens, ftruncate, fallocate, fgetattr, fsync, ioctl (files), read, readdir, release, statfs, write, copy_file_range |

In cases where something may be searched for (such as a path to clone)
`getattr` will usually be used.


## Policies

See below for [available policies and their descriptions](#policy-descriptions).

A policy is an algorithm designed to select one or more branches for a
function to operate on.

Policies do not actually manage the filesystem or layout. They are
strictly responsible for deciding which files or branches will be
worked on in relation to the function being performed. Once the branch
is chosen other parts of the system does what is necessary to
accomplish the function. Such as the cloning of directories between
branches.

When using policies which are based on a branch's available space the
branch base path provided is used. Not the full path to the file or
directory in question. Meaning that mounts within the branch will not
be considered in the space calculations.

**NOTE:** While any policy can be assigned to a function or category,
some may not be very useful in practice. For instance: `rand` (random)
may be useful for file creation but could lead to very odd behavior if
used for `chmod` if there were more than one copy of the file. Unless
users find this flexibility useful it will likely be removed in the
future.


## Filtering

Most policies search branches and create a list of files / paths for
functions to work on. The policy is responsible for filtering and
sorting the branches. Filters include [minfreespace](minfreespace.md),
whether or not a branch is mounted read-only, and the branch mode
(RO,NC,RW). These filters are applied across most policies.

- No `search` function policies filter.
- All `action` function policies filter out branches which are
  mounted **read-only** or mode is **RO (read-only)**.
- All `create` function policies filter out branches which are
  mounted **read-only**, mode **RO (read-only)** or **NC (no
  create)**, or has available space less than
  [minfreespace](minfreespace.md).

Policies may have their own additional filtering such as those that
require relative paths to be present.

If all branches are filtered an error will be returned. Typically
`EROFS` (read-only filesystem) or `ENOSPC` (no space left on
device) depending on the most recent reason for filtering a
branch.

If `create`, `mkdir`, `mknod`, or `symlink` fail with `EROFS`
or other fundamental errors then mergerfs will mark any branch found
to be read-only as such (IE will set the mode `RO`) and will rerun the
policy and try again. This is mostly for `ext4` filesystems that can
suddenly become read-only when it encounters an error.


## Path Preservation

Policies, as described below, are of two basic classifications. `path
preserving` and `non-path preserving`.

All policies which start with `ep` (`epff`, `eplfs`, `eplus`, `epmfs`,
`eprand`) are `path preserving`. `ep` stands for `existing path`.

A path preserving policy will only consider branches where the relative
path being accessed already exists.

With the `msp` or `most shared path` policies they are defined as
`path preserving` for the purpose of controlling `link` and `rename`'s
behaviors since `ignorepponrename` is available to disable that
behavior.


## Policy descriptions

A policy's behavior differs, as mentioned above, based on the function
it is used with. Sometimes it really might not make sense to even
offer certain policies because they are literally the same as others
but it makes things a bit more uniform.

| Policy                                                          | Description                                                                                                                                                                     |
| --------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| pfrd (percentage free random distribution)                      | Selects a branch randomly, with the probability of selection proportional to its available space relative to the total available space across all branches. For instance, if Branch A has 100 GB free and Branch B has 50 GB free, Branch A is twice as likely to be chosen. |
| rand (random)                                                   | Calls **all** and then randomizes. Returns 1 branch.                                                                                                                            |
| mfs (most free space)                                           | Pick the branch with the most available free space.                                                                                                                             |
| ff (first found)                                                | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found.                                                                 |
| lfs (least free space)                                          | Pick the branch with the least available free space.                                                                                                                            |
| lup (least used percent)                                        | Pick the branch with the least used 
percentage of space                                                                                                                         |
| lus (least used space)                                          | Pick the branch with the least used space.                                                                                                                                      |
| all                                                             | Search: For **mkdir**, **mknod**, and **symlink** it will apply to all branches. **create** works like **ff**.                                                                  |
| msppfrd (most shared path, percentage free random distribution) | Like **eppfrd** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                          |
| mspmfs (most shared path, most free space)                      | Like **epmfs** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| msplfs (most shared path, least free space)                     | Like **eplfs** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| msplus (most shared path, least used space)                     | Like **eplus** but if it fails to find a branch it will try again with the parent directory. Continues this pattern till finding one.                                           |
| eppfrd (existing path, percentage free random distribution)     | Like **pfrd** but limited to existing paths.                                                                                                                                    |
| epmfs (existing path, most free space)                          | Of all the branches on which the relative path exists choose the branch with the most free space.                                                                               |
| eprand (existing path, random)                                  | Calls **epall** and then randomizes. Returns 1.                                                                                                                                 |
| epff (existing path, first found)                               | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found where the relative path exists.                                  |
| eplfs (existing path, least free space)                         | Of all the branches on which the relative path exists choose the branch with the least free space.                                                                              |
| eplus (existing path, least used space)                         | Of all the branches on which the relative path exists choose the branch with the least used space.                                                                              |
| epall (existing path, all)                                      | For **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **epff** (but more expensive because it doesn't stop after finding a valid branch). |
| newest                                                          | Pick the file / directory with the largest mtime.                                                                                                                               |

**NOTE:** If you are using an underlying filesystem that reserves
blocks such as ext2, ext3, or ext4 be aware that mergerfs respects the
reservation by using `f_bavail` (number of free blocks for
unprivileged users) rather than `f_bfree` (number of free blocks) in
policy calculations. **df** does NOT use `f_bavail`, it uses
`f_bfree`, so direct comparisons between **df** output and mergerfs'
policies is not appropriate.


## Defaults

| Category | Policy |
| -------- | ------ |
| action   | epall  |
| create   | pfrd   |
| search   | ff     |

# "Why isn't it working?"

## I modified mergerfs' config but it still behaves the same.

mergerfs, like other filesystems, are given their options/arguments at
mount time. You can not simply modify the [source of the
configuration](../quickstart.md#usage) and have those settings applied
any more than you would for other filesystems. It is the user's
responsibility to [restart](../setup/upgrade.md) mergerfs to pick up
the changes or use the [runtime interface](../runtime_interfaces.md).

NOTE: the [runtime interface](../runtime_interfaces.md) is **just**
for runtime changes. It does **NOT** save those changed values
anywhere.


## Why are all my files ending up on 1 filesystem?!

Did you start with empty filesystems? Are you using an `existing path`
policy?

The default create policy is `epmfs`. That is a path preserving
algorithm. With such a policy for `mkdir` and `create` with a set of
empty filesystems it will select only 1 filesystem when the first
directory is created. Anything, files or directories, created in that
directory will be placed on the same branch because it is preserving
paths. That is the expected behavior.

This may catch new users off guard but this policy is the safest
policy to have as default as it will not change the general layout of
the underlying branches. If you do not care about path preservation
([most should
not](configuration_and_policies.md#how-can-i-ensure-files-are-collocated-on-the-same-branch))
and wish your files to be spread across all your filesystems change
`create.category` to `pfrd`, `rand`, `mfs` or a similarly non-path
restrictive [policy](../config/functions_categories_policies.md).


## Why do I get an "out of space" / "no space left on device" / ENOSPC error even though there appears to be lots of space available?

First make sure you've read the sections above about [policies, path
preservation, branch
filtering,](../config/functions_categories_policies.md) and the
options `minfreespace`, [moveonenospc](../config/moveonenospc.md),
[statfs](../config/statfs.md), and
[statfs_ignore](../config/statfs.md#statfs_ignore).

mergerfs is simply presenting a union of the content within multiple
branches. The reported free space is an aggregate of space available
within the pool (behavior modified by `statfs` and
`statfs_ignore`). It does not represent a contiguous space. In the
same way that read-only filesystems, those with quotas, or reserved
space report the full theoretical space available. Not the practical
usable space.

Due to using an `existing path` based policy, setting a [branch's
mode](../config/branches.md#branch-mode) to `NC` or `RO`, a
filesystems read-only status, and/or `minfreespace` setting it is
[perfectly
valid](../config/functions_categories_policies.md#filtering) that
`ENOSPC` / "out of space" / "no space left on device" be returned when
attempting to create a file despite there being actual space available
somewhere in the pool. It is doing what was asked of it: filtering
possible branches due to those settings. If that is not the behavior
you want you need to modify the settings accordingly.

It is also possible that the filesystem selected has run out of
inodes. Use `df -i` to list the total and available inodes per
filesystem.


## Why isn't the create policy working?

It probably is. The policies rather straight forward and well tested.

First, confirm the policy is configured as expected by using the
[runtime interface](../runtime_interfaces.md).

```shell
$ getfattr -n user.mergerfs.category.create /mnt/mergerfs/.mergerfs
# file: mnt/mergerfs/.mergerfs
user.mergerfs.category.create="mfs"
```

Second, as discussed in the [support](../support.md) section, test the
behavior using simple command line tools such as `touch` and then see
where it was created.


```shell
$ touch /mnt/mergerfs/new-file
$ getfattr -n user.mergerfs.allpaths /mnt/mergerfs/new-file
# file: mnt/mergerfs/new-file
user.mergerfs.allpaths="/mnt/hdd/drive1/new-file"
```

If the location of the file is where it should be according to the
state of the system at the time and the policy selected then the
"problem" lies elsewhere.

[Keep in
mind](technical_behavior_and_limitations.md/#how-does-mergerfs-handle-moving-and-copying-of-files)
that files, when created, have no size. If a number of files are
created at the same time, for example by a program downloading
numerous files like a BitTorrent client, then depending on the policy
the files could be created on the same branch. As the files are
written to, or resized immediately afterwards to the total size of the
file being downloaded, the files will take up more space but there is
no mechanism to move them as they grow. Nor would it be a good idea to
do so as it would be expensive to continuously calculate their size
and perform the move while the file is still being written to. As such
an imbalance will occur that wouldn't if the files had been created
and resized one at a time.

If you wish to reduce the likelihood of this happening a policy that
does not make decisions on available space alone such as `pfrd` or
`rand` should be used.


## Why can't I see my files / directories?

It's almost always a permissions issue. Unlike mhddfs and
unionfs-fuse, which accesses content as root, mergerfs always changes
its credentials to that of the caller. This is done as it is the only
properly secure way to manage permissions. This means that if the user
does not have access to a file or directory than neither will
mergerfs. However, because mergerfs is creating a union of paths it
may be able to read some files and directories on one filesystem but
not another resulting in an incomplete set. And if one of the branches
it can access is empty then it will return an empty list.

Try using [mergerfs.fsck](https://github.com/trapexit/mergerfs-tools)
tool to check for and fix inconsistencies in permissions. If you
aren't seeing anything at all be sure that the basic permissions are
correct. The user and group values are correct and that directories
have their executable bit set. A common mistake by users new to Linux
is to `chmod -R 644` when they should have `chmod -R u=rwX,go=rX`.

If using a [network filesystem](../remote_filesystems.md) such as NFS
or SMB (Samba) be sure to pay close attention to anything regarding
permissioning and users. Root squashing and user translation for
instance has bitten a few mergerfs users. Some of these also affect
the use of mergerfs from [container platforms such as
Docker.](compatibility_and_integration.md)


## Why are file moves, renames, links/hardlinks failing?

mergerfs fully supports rename and link functions to the degree that
is possible in a union filesystem. Problems related to rename and link
are almost exclusively due to incorrect setup of software or poorly
written software.


### Docker, Podman, and other container runtimes

Keep in mind that [rename and link](../config/rename_and_link.md) will
**NOT** work across devices. That includes between the original
filesystem and a mergerfs pool, between two separate pools of the same
underlying filesystems, or bind mounts of paths within the mergerfs
pool. The latter is common when using Docker, Podman, or other
container runtimes. Multiple volumes (bind mounts) to the same
underlying filesystem are considered different devices. There is no
way to link or rename between them despite them binding to the same
underlying filesystem. Neither mergerfs or any other filesystem has a
say in this behavior as the kernel is erroring out before the request
even gets to the filesystems in question.

To work around this situation the highest directory in the mergerfs
pool that includes all the paths you need is what should be bind'ed
into the container if you want links and rename to work between the
paths in question.

This is a common problem for users of Radarr, Sonarr, Lidarr,
etc. when they use the default volume mount options suggested by
projects like
[LinuxServer.io](https://docs.linuxserver.io/images/docker-radarr/?h=radarr#docker-compose-recommended-click-here-for-more-info). Do
not use separate "movies" and "downloads" volumes.

```
# NO
volumes:
  - /mnt/pool/movies:/movies
  - /mnt/pool/downloads:/downloads

# YES
volumes:
  - /mnt/pool:/media
# Then use /media/movies and /media/downloads in your software.
```


### Path-Preserving Create Policies

As described in the [rename and link](../config/rename_and_link.md)
docs if a path-preserving create
[policy](../config/functions_categories_policies.md) is configured
then mergerfs, in order to follow that restrictive policy, must return
an error
([EXDEV](https://man7.org/linux/man-pages/man2/rename.2.html)) if the
target path of a rename or link is not found on the target branch.

```
EXDEV  oldpath and newpath are not on the same mounted filesystem.
       (Linux permits a filesystem to be mounted at multiple
       points, but rename() does not work across different mount
       points, even if the same filesystem is mounted on both.)
```

[Most
users](configuration_and_policies.md#how-can-i-ensure-files-are-collocated-on-the-same-branch)
do not have a need for path-preserving policies but if you intend on
using path-preserving policies you must accept that `EXDEV` errors are
more likely to occur. You could enable
[ignorepponrename](../config/options.md) however doing so would
contradict the intention of using a path restrictive policy.


### Poorly written software

There are some pieces of software that do not properly handle `EXDEV`
errors which `rename` and `link` may return even though they are
perfectly valid situations and do not indicate actual device,
filesystem, or OS problems. The error will rarely be returned by
mergerfs if using a non-path preserving
[policy](../config/functions_categories_policies.md) however there are
edge cases where it may (such as mounting another filesystem into the
mergerfs pool.) If you do not care about path preservation set the
mergerfs policy to the non-path preserving variant. For example: `-o
category.create=pfrd`. Ideally the offending software would be fixed
and it is recommended that if you run into this problem you contact
the software's author and request proper handling of `EXDEV` errors.


### Additional Reading

* [rename and link](../config/rename_and_link.md)
* [Do hardlinks work?](technical_behavior_and_limitations.md#do-hardlinks-work)
* [How does mergerfs handle moving and copying of files?](technical_behavior_and_limitations.md#how-does-mergerfs-handle-moving-and-copying-of-files)
* [Moving files and directories fails with Samba](../known_issues_bugs.md#moving-files-and-directories-fails-with-samba)


## Why is rtorrent failing with ENODEV (No such device)?

Be sure to set
[cache.files=partial|full|auto-full|per-process](../config/cache.md)
or use Linux kernel v6.6 or above. rtorrent and some other
applications use [mmap](http://linux.die.net/man/2/mmap) to read and
write to files and offer no fallback to traditional methods.


## Why don't I see mergerfs options in mount command or /proc/mounts?

FUSE filesystems, such as mergerfs, are interpreting most of the
possible options rather than being given to the kernel where those
options in the `mount` command and /proc/mounts come from.

If you want to see the options of a running instance of mergerfs you
can use the [runtime interface](../runtime_interfaces.md).

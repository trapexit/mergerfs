# "Why isn't it working?"

## I modified mergerfs' config but it still behaves the same.

mergerfs, like most filesystems, are given their options/arguments
at mount time. Unlike most filesystems, mergerfs also has the ability
to modify certain options at [runtime](../runtime_interfaces.md).

That said: mergerfs does not actively try to monitor typical methods
of configuration (nor should it.) As such if changes are made to
`/etc/fstab`, a systemd unit file, etc. it will have no knowledge of
those changes. It is the user's responsibility to
[restart](../setup/upgrade.md) mergerfs to pick up the changes or use
the [runtime interface](../runtime_interfaces.md).


## Why are all my files ending up on 1 filesystem?!

Did you start with empty filesystems? Did you explicitly configure a
`category.create` policy? Are you using an `existing path` / `path
preserving` policy?

The default create policy is `epmfs`. That is a path preserving
algorithm. With such a policy for `mkdir` and `create` with a set of
empty filesystems it will select only 1 filesystem when the first
directory is created. Anything, files or directories, created in that
directory will be placed on the same branch because it is preserving
paths. That is the expected behavior.

This may catch new users off guard but this policy is the safest
policy to start with as it will not change the general layout of the
underlying branches. If you do not care about path preservation ([most
should
not](configuration_and_policies.md#how-can-i-ensure-files-are-collocated-on-the-same-branch))
and wish your files to be spread across all your filesystems change to
`pfrd`, `rand`, `mfs` or similar
[policy](../config/functions_categories_and_policies.md).


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
do so as it would be expensive to continuously calculate their size and
perform the move while the file is still being written to. As such an
imbalance will occur that wouldn't if the files had been downloaded
one at a time.

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

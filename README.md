% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2016-02-21

# NAME

mergerfs - another (FUSE based) union filesystem

# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;srcmounts&gt; &lt;mountpoint&gt;

# DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifing storage and management of files across numerous commodity storage devices. It is similar to **mhddfs**, **unionfs**, and **aufs**.

# FEATURES

* Runs in userspace (FUSE)
* Configurable behaviors
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Runtime configurable (via xattrs)
* Safe to run as root
* Opportunistic credential caching
* Works with heterogeneous filesystem types
* Handling of writes to full drives
* Handles pool of readonly and read/write drives

# OPTIONS

###options###

* **defaults**: a shortcut for FUSE's **atomic_o_trunc**, **auto_cache**, **big_writes**, **default_permissions**, **splice_move**, **splice_read**, and **splice_write**. These options seem to provide the best performance.
* **direct_io**: causes FUSE to bypass an addition caching step which can increase write speeds at the detriment of read speed. 
* **minfreespace**: the minimum space value used for creation policies. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively. (default: 4G)
* **moveonenospc**: when enabled (set to **true**) if a **write** fails with **ENOSPC** a scan of all drives will be done looking for the drive with most free space which is at least the size of the file plus the amount which failed to write. An attempt to move the file to that drive will occur (keeping all metadata possible) and if successful the original is unlinked and the write retried. (default: false)
* **func.&lt;func&gt;=&lt;policy&gt;**: sets the specific FUSE function's policy. See below for the list of value types. Example: **func.getattr=newest**
* **category.&lt;category&gt;=&lt;policy&gt;**: Sets policy of all FUSE functions in the provided category. Example: **category.create=mfs**

**NOTE:** Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.

###srcmounts###

The source mounts argument is a colon (':') delimited list of paths. To make it simpler to include multiple source mounts without having to modify your [fstab](http://linux.die.net/man/5/fstab) we also support [globbing](http://linux.die.net/man/7/glob). **The globbing tokens MUST be escaped when using via the shell else the shell itself will probably expand it.**

```
$ mergerfs /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all mount points in /mnt prefixed with *disk* and the directory *cdrom*.

In /etc/fstab it'd look like the following:

```
# <file system>        <mount point>  <type>         <options>             <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  defaults,allow_other  0       0
```

**NOTE:** the globbing is done at mount or xattr update time. If a new directory is added matching the glob after the fact it will not be included.

# FUNCTIONS / POLICIES / CATEGORIES

The filesystem has a number of functions. Those functions are grouped into 3 categories: **action**, **create**, **search**. These functions and categories can be assigned a policy which dictates how **mergerfs** behaves. Any policy can be assigned to a function or category though some are not very practical. For instance: **rand** (Random) may be useful for file creation (create) but could lead to very odd behavior if used for `chmod`.

All policies when used to create will ignore drives which are mounted readonly. This allows for read/write and readonly drives to be mixed together.

#### Function / Category classifications ####

| Category | FUSE Functions |
|----------|----------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink |
| search   | access, getattr, getxattr, ioctl, listxattr, open, readlink |
| N/A      | fallocate, fgetattr, fsync, ftruncate, ioctl, read, readdir, release, statfs, write |

Due to FUSE limitations **ioctl** behaves differently if its acting on a directory. It'll use the **getattr** policy to find and open the directory before issuing the **ioctl**. In other cases where something may be searched (to confirm a directory exists across all source mounts) then **getattr** will be used.

#### Policy descriptions ####

Most policies when called to create will filter out drives which are readonly or have less than **minfreespace**.

| Policy | Description |
|--------------|-------------|
| all | Search category: acts like **ff**. Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** perform on all read/write drives with **minfreespace**. **create** filters the same way but acts like **ff**. |
| eplfs (existing path, least free space) | If the path exists on multiple drives use the one with the least free space. Falls back to **lfs**. |
| epmfs (existing path, most free space) | If the path exists on multiple drives use the one with the most free space. Falls back to **mfs**. |
| erofs | Exclusively return **-1** with **errno** set to **EROFS**. By setting **create** functions to this you can in effect turn the filesystem readonly. |
| ff (first found) | Given the order of the drives, as defined at mount time or when configured via xattr interface, act on the first one found. |
| lfs (least free space) | Pick the drive with the least available free space but more than **minfreespace**. Falls back to **mfs**. |
| mfs (most free space) | Use the drive with the most available free space. Falls back to **ff**. |
| newest (newest file) | Pick the file / directory with the largest mtime. |
| rand (random) | Calls **all** and then randomizes. |

#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | all    |
| create   | epmfs  |
| search   | ff     |

#### rename & link ####

[rename](http://man7.org/linux/man-pages/man2/rename.2.html) is a tricky function in a merged system. Normally if a rename can't be done atomically due to the source and destination paths existing on different mount points it will return `-1` with `errno = EXDEV`. The atomic rename is most critical for replacing files in place atomically (such as securing writing to a temp file and then replacing a target). The problem is that by merging multiple paths you can have N instances of the source and destinations on different drives. This can lead to several undesirable situtations with or without errors and it's not entirely obvious what to do when an error occurs.

Originally mergerfs would return EXDEV whenever a rename was requested which was cross directory in any way. This made the code simple and was technically complient with POSIX requirements. However, many applications fail to handle EXDEV at all and treat it as a normal error or they only partially support EXDEV (don't respond the same as `mv` would). Such apps include: gvfsd-fuse v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+, NZBGet, Samba's recycling bin feature.

* If using a `create` policy which tries to preserve directory paths (epmfs,eplfs)
  * Using the `rename` policy get the list of files to rename
  * For each file attempt rename:
    * If failure with ENOENT run `create` policy
    * If create policy returns the same drive as currently evaluating then clone the path
    * Re-attempt rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all drives with no source file
    * Remove the source from all drives which failed to rename
* If using a `create` policy which does **not** try to preserve directory paths
  * Using the `rename` policy get the list of files to rename
  * Using the `getattr` policy get the target path
  * For each file attempt rename:
    * If the source drive != target drive:
      * Clone target path from target drive to source drive
    * Rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all drives with no source file
    * Remove the source from all drives which failed to rename

The the removals are subject to normal entitlement checks.

The above behavior will help minimize the likelihood of EXDEV being returned but it will still be possible. To remove the possibility all together mergerfs would need to perform the as `mv` does when it receives EXDEV normally.

`link` uses the same basic strategy.

#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is different from all other filesystem functions. It certainly could have it's own set of policies to tweak its behavior. At this time it provides a simple **first found** merging of directories and files found. That is: only the first file or directory found for a directory is returned. Given how FUSE works though the data representing the returned entry comes from **getattr**.

It could be extended to offer the ability to see all files found. Perhaps concatenating **#** and a number to the name. But to really be useful you'd need to be able to access them which would complicate file lookup.

#### statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple sources on the same drive will not result in double counting it's space.

# BUILDING

**NOTE:** Prebuilt packages can be found at: https://github.com/trapexit/mergerfs/releases

First get the code from [github](http://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/archive/master.zip
```

#### Debian / Ubuntu
```
$ sudo apt-get install g++ pkg-config git git-buildpackage pandoc debhelper libfuse-dev libattr1-dev python
$ cd mergerfs
$ make deb
$ sudo dpkg -i ../mergerfs_version_arch.deb
```

#### Fedora
```
$ su -
# dnf install rpm-build fuse-devel libattr-devel pandoc gcc-c++ git make which python
# cd mergerfs
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<verion>.<arch>.rpm
```

#### Generically

Have git, python, pkg-config, pandoc, libfuse, libattr1 installed.

```
$ cd mergerfs
$ make
$ make man
$ sudo make install
```

# RUNTIME

#### .mergerfs pseudo file ####
```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for the runtime modification of certain **mergerfs** options. The file will not show up in **readdir** but can be **stat**'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Even if xattrs are disabled the [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls will still work.

##### Keys #####

Use `xattr -l /mount/point/.mergerfs` to see all supported keys.

##### user.mergerfs.srcmounts #####

For **user.mergerfs.srcmounts** there are several instructions available for manipulating the list. The value provided is just as the value used at mount time. A colon (':') delimited list of full path globs.

| Instruction | Description |
|--------------|-------------|
| [list]       | set |
| +<[list]     | prepend |
| +>[list]     | append |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |

##### minfreespace #####

Input: interger with an optional multiplier suffix. **K**, **M**, or **G**.

Output: value in bytes

##### moveonenospc #####

Input: **true** and **false**

Ouput: **true** or **false**

##### categories / funcs #####

Input: short policy string as described elsewhere in this document

Output: the policy string except for categories where its funcs have multiple types. In that case it will be a comma separated list

##### Example #####

```
[trapexit:/tmp/mount] $ xattr -l .mergerfs
user.mergerfs.srcmounts: /tmp/a:/tmp/b
user.mergerfs.minfreespace: 4294967295
user.mergerfs.moveonenospc: false
...

[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.category.search newest .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.category.search .mergerfs
newest

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts +/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts =/tmp/c .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/c

[trapexit:/tmp/mount] $ xattr -w user.mergerfs.srcmounts '+</tmp/a:/tmp/b' .mergerfs
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.srcmounts .mergerfs
/tmp/a:/tmp/b:/tmp/c
```

#### mergerfs file xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) **mergerfs** offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* **user.mergerfs.basepath:** the base mount point for the file given the current getattr policy
* **user.mergerfs.relpath:** the relative path of the file from the perspective of the mount point
* **user.mergerfs.fullpath:** the full path of the original file given the getattr policy
* **user.mergerfs.allpaths:** a NUL ('\0') separated list of full paths to all files found

```
[trapexit:/tmp/mount] $ ls
A B C
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.basepath A
/mnt/a
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.relpath A
/full/path/to/A
[trapexit:/tmp/mount] $ xattr -p user.mergerfs.allpaths A | tr '\0' '\n'
/mnt/a/full/path/to/A
/mnt/b/full/path/to/A
```

# TOOLING

Find tooling to help with managing `mergerfs` at: https://github.com/trapexit/mergerfs-tools

* mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
* mergerfs.mktrash: Creates FreeDesktop.org Trash specification compatible directories on a mergerfs mount

# TIPS / NOTES

* Detailed guides to setting up a backup solution using mergerfs and other technologies: https://github.com/trapexit/backup-and-recovery-howtos
* If you don't see some directories / files you expect in a merged point be sure the user has permission to all the underlying directories. If `/drive0/a` has is owned by `root:root` with ACLs set to `0700` and `/drive1/a` is `root:root` and `0755` you'll see only `/drive1/a`. Use `mergerfs.fsck` to audit the drive for out of sync permissions.
* Since POSIX gives you only error or success on calls its difficult to determine the proper behavior when applying the behavior to multiple targets. **mergerfs** will return an error only if all attempts of an action fail. Any success will lead to a success returned.
* The recommended options are **defaults,allow_other**. The **allow_other** is to allow users who are not the one which executed mergerfs access to the mountpoint. **defaults** is described above and should offer the best performance. It's possible that if you're running on an older platform the **splice** features aren't available and could error. In that case simply use the other options manually.
* If write performance is valued more than read it may be useful to enable **direct_io**. Best to benchmark with and without and choose appropriately.
* Remember: some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* An example: [Kodi](http://kodi.tv) and [Plex](http://plex.tv) can use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether to scan for new content rather than simply performing a full scan. If using the current default **getattr** policy of **ff** its possible **Kodi** will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Due to previously mentioned issues its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync).

# KNOWN ISSUES / BUGS

#### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our case that will really only happen with path preserving policies like `epmfs`) and the software doesn't handle the situtation well. This is unfortunately a common failure of software which moves files around. The standard indicates that an implementation `MAY` choose to support non-user home directory trashing of files (which is a `MUST`). The implementation `MAY` also support "top directory trashes" which many probably do. 

To create a `$topdir/.Trash` directory as defined in the standard use the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool `mergerfs.mktrash`.

#### Samba: Moving files / directories fails

Workaround: Copy the file/directory and then remove the original rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3 and prior (found in Ubuntu 14.04 among others) failed to handle certain error codes correctly. Particularly **STATUS_NOT_SAME_DEVICE** which comes from the **EXDEV** which is returned by **rename** when the call is crossing mount points. When a program gets an **EXDEV** it needs to explicitly take an alternate action to accomplish it's goal. In the case of **mv** or similar it tries **rename** and on **EXDEV** falls back to a manual copying of data between the two locations and unlinking the source. In these older versions of GVFS-fuse if it received **EXDEV** it would translate that into **EIO**. This would cause **mv** or most any application attempting to move files around on that SMB share to fail with a IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568) and above fixed this issue but a large number of systems use the older release. On Ubuntu the version can be checked by issuing `apt-cache showpkg gvfs-fuse`. Most distros released in 2015 seem to have the updated release and will work fine but older systems may not. Upgrading gvfs-fuse or the distro in general will address the problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with their own product. It appears their new client does not handle **EXDEV** either and responds similar to older release of gvfs on Linux.

#### Supplemental user groups

Due to the overhead of [getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs utilizes a cache. This cache is opportunistic and per thread. Each thread will query the supplemental groups for a user when that particular thread needs to change credentials and will keep that data for the lifetime of the thread. This means that if a user is added to a group it may not be picked up without the restart of mergerfs. However, since the high level FUSE API's (at least the standard version) thread pool dynamically grows and shrinks it's possible that over time a thread will be killed and later a new thread with no cache will start and query the new data.

The gid cache uses fixed storage to simplify the design and be compatible with older systems which may not have C++11 compilers. There is enough storage for 256 users' supplemental groups. Each user is allowed upto 32 supplemental groups. Linux >= 2.6.3 allows upto 65535 groups per user but most other *nixs allow far less. NFS allowing only 16. The system does handle overflow gracefully. If the user has more than 32 supplemental groups only the first 32 will be used. If more than 256 users are using the system when an uncached user is found it will evict an existing user's cache at random. So long as there aren't more than 256 active users this should be fine. If either value is too low for your needs you will have to modify `gidcache.hpp` to increase the values. Note that doing so will increase the memory needed by each thread.

#### mergerfs or libfuse crashing

If suddenly the mergerfs mount point disappears and `Transport endpoint is not connected` is returned when attempting to perform actions within the mount directory **and** the version of libfuse (use `mergerfs -v` to find the version) is older than `2.9.4` its likely due to a bug in libfuse. Affected versions of libfuse can be found in Debian Wheezy, Ubuntu Precise and others.

In order to fix this please install newer versions of libfuse. If using a Debian based distro (Debian,Ubuntu,Mint) you can likely just install newer versions of [libfuse](https://packages.debian.org/unstable/libfuse2) and [fuse](https://packages.debian.org/unstable/fuse) from the repo of a newer release.

# FAQ

#### Why use mergerfs over mhddfs?

mhddfs is no longer maintained and has some known stability and security issues (see below).

#### Why use mergerfs over aufs?

While aufs can offer better peak performance mergerfs offers more configurability and is generally easier to use. mergerfs however doesn't offer the overlay features which tends to result in whiteout files being left around the underlying filesystems.

#### Why use mergerfs over LVM/ZFS/BTRFS/RAID0 drive concatenation / striping?

A single drive failure will lead to full pool failure without additional redundancy. mergerfs performs a similar behavior without the catastrophic failure and lack of recovery. Drives can fail and all other data will continue to be accessable.

#### It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?

[mhddfs](https://github.com/trapexit/mhddfs) tries to handle being run as **root** by calling [getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319) and if it returns **0** then it will [chown](http://linux.die.net/man/1/chown) the file. Not only is that a race condition but it doesn't handle many other situations. Rather than attempting to simulate POSIX ACL behaviors the proper behavior is to use [seteuid](http://linux.die.net/man/2/seteuid) and [setegid](http://linux.die.net/man/2/setegid), become the user making the original call and perform the action as them. This is how [mergerfs](https://github.com/trapexit/mergerfs) handles things.

If you are familiar with POSIX standards you'll know that this behavior poses a problem. **seteuid** and **setegid** affect the whole process and **libfuse** is multithreaded by default. We'd need to lock access to **seteuid** and **setegid** with a mutex so that the several threads aren't stepping on one anofther and files end up with weird permissions and ownership. This however wouldn't scale well. With lots of calls the contention on that mutex would be extremely high. Thankfully on Linux and OSX we have a better solution.

OSX has a [non-portable pthread extension](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/pthread_setugid_np.2.html) for per-thread user and group impersonation.

Linux does not support [pthread_setugid_np](https://developer.apple.com/library/mac/documentation/Darwin/Reference/ManPages/man2/pthread_setugid_np.2.html) but user and group IDs are a per-thread attribute though documentation on that fact or how to manipulate them is not well distributed. From the **4.00** release of the Linux man-pages project for [setuid](http://man7.org/linux/man-pages/man2/setuid.2.html).

> At the kernel level, user IDs and group IDs are a per-thread attribute.  However, POSIX requires that all threads in a process share the same credentials.  The NPTL threading implementation handles the POSIX requirements by providing wrapper functions for the various system calls that change process UIDs and GIDs.  These wrapper functions (including the one for setuid()) employ a signal-based technique to ensure that when one thread changes credentials, all of the other threads in the process also change their credentials.  For details, see nptl(7).

Turns out the setreuid syscalls apply only to the thread. GLIBC hides this away using RT signals to inform all threads to change credentials. Taking after **Samba** mergerfs uses **syscall(SYS_setreuid,...)** to set the callers credentials for that thread only. Jumping back to **root** as necessary should escalated privileges be needed (for instance: to clone paths).

For non-Linux systems mergerfs uses a read-write lock and changes credentials only when necessary. If multiple threads are to be user X then only the first one will need to change the processes credentials. So long as the other threads need to be user X they will take a readlock allow multiple threads to share the credentials. Once a request comes in to run as user Y that thread will attempt a write lock and change to Y's credentials when it can. If the ability to give writers priority is supported then that flag will be used so threads trying to change credentials don't starve. This isn't the best solution but should work reasonably well. As new platforms are supported if they offer per thread credentials those APIs will be adopted.

# SUPPORT

#### Issues with the software
* github.com: https://github.com/trapexit/mergerfs/issues
* email: trapexit@spawn.link

#### Support development
* Gratipay: https://gratipay.com/~trapexit
* BitCoin: 12CdMhEPQVmjz3SSynkAEuD5q9JmhTDCZA

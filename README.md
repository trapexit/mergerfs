% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2016-10-13

# NAME

mergerfs - another (FUSE based) union filesystem

# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;srcmounts&gt; &lt;mountpoint&gt;

# DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifying storage and management of files across numerous commodity storage devices. It is similar to **mhddfs**, **unionfs**, and **aufs**.

# FEATURES

* Runs in userspace (FUSE)
* Configurable behaviors
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Runtime configurable (via xattrs)
* Safe to run as root
* Opportunistic credential caching
* Works with heterogeneous filesystem types
* Handling of writes to full drives (transparently move file to drive with capacity)
* Handles pool of readonly and read/write drives

# OPTIONS

###options###

* **defaults**: a shortcut for FUSE's **atomic_o_trunc**, **auto_cache**, **big_writes**, **default_permissions**, **splice_move**, **splice_read**, and **splice_write**. These options seem to provide the best performance.
* **direct_io**: causes FUSE to bypass an addition caching step which can increase write speeds at the detriment of read speed. 
* **minfreespace**: the minimum space value used for creation policies. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively. (default: 4G)
* **moveonenospc**: when enabled (set to **true**) if a **write** fails with **ENOSPC** or **EDQUOT** a scan of all drives will be done looking for the drive with most free space which is at least the size of the file plus the amount which failed to write. An attempt to move the file to that drive will occur (keeping all metadata possible) and if successful the original is unlinked and the write retried. (default: false)
* **func.&lt;func&gt;=&lt;policy&gt;**: sets the specific FUSE function's policy. See below for the list of value types. Example: **func.getattr=newest**
* **category.&lt;category&gt;=&lt;policy&gt;**: Sets policy of all FUSE functions in the provided category. Example: **category.create=mfs**
* **fsname**: sets the name of the filesystem as seen in **mount**, **df**, etc. Defaults to a list of the source paths concatenated together with the longest common prefix removed.

**NOTE:** Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.

###srcmounts###

The srcmounts (source mounts) argument is a colon (':') delimited list of paths to be included in the pool. It does not matter if the paths are on the same or different drives nor does it matter the filesystem. Used and available space will not be duplicated for paths on the same device and any features which aren't supported by the underlying filesystem (such as file attributes or extended attributes) will return the appropriate errors.

To make it easier to include multiple source mounts mergerfs supports [globbing](http://linux.die.net/man/7/glob). **The globbing tokens MUST be escaped when using via the shell else the shell itself will expand it.**

```
$ mergerfs -o defaults,allow_other /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all mount points in /mnt prefixed with **disk** and the **cdrom**.

To have the pool mounted at boot or otherwise accessable from related tools use **/etc/fstab**.

```
# <file system>        <mount point>  <type>         <options>             <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /media/drives  fuse.mergerfs  defaults,allow_other  0       0
```

**NOTE:** the globbing is done at mount or xattr update time (see below). If a new directory is added matching the glob after the fact it will not be automatically included.

**NOTE:** for mounting via **fstab** to work you must have **mount.fuse** installed. For Ubuntu/Debian it is included in the **fuse** package.

# FUNCTIONS / POLICIES / CATEGORIES

The POSIX filesystem API has a number of functions. **creat**, **stat**, **chown**, etc. In mergerfs these functions are grouped into 3 categories: **action**, **create**, and **search**. Functions and categories can be assigned a policy which dictates how **mergerfs** behaves. Any policy can be assigned to a function or category though some are not very practical. For instance: **rand** (random) may be useful for file creation (create) but could lead to very odd behavior if used for `chmod` (though only if there were more than one copy of the file).

Policies, when called to create, will ignore drives which are readonly or have less than **minfreespace**. This allows for read/write and readonly drives to be mixed together and keep drives which may remount as readonly on error from further affecting the pool.

#### Function / Category classifications ####

| Category | FUSE Functions |
|----------|----------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink |
| search   | access, getattr, getxattr, ioctl, listxattr, open, readlink |
| N/A      | fallocate, fgetattr, fsync, ftruncate, ioctl, read, readdir, release, statfs, write |

Due to FUSE limitations **ioctl** behaves differently if its acting on a directory. It'll use the **getattr** policy to find and open the directory before issuing the **ioctl**. In other cases where something may be searched (to confirm a directory exists across all source mounts) **getattr** will also be used.

#### Policy descriptions ####

| Policy | Description |
|--------------|-------------|
| all | Search category: acts like **ff**. Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **ff**. It will exclude readonly drives and those with free space less than **minfreespace**. |
| epall (existing path, all) | Search category: acts like **epff**. Action category: apply to all found. Create category: for **mkdir**, **mknod**, and **symlink** it will apply to all existing paths found. **create** works like **epff**. It will exclude readonly drives and those with free space less than **minfreespace**. |
| epff | Given the order of the drives, as defined at mount time or when configured via the xattr interface, act on the first one found where the path already exists. For **create** category it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). Falls back to **ff**. |
| eplfs (existing path, least free space) | If the path exists on multiple drives use the one with the least free space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **lfs**. |
| eplus (existing path, least used space) | If the path exists on multiple drives use the one with the least used space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **lus**. |
| epmfs (existing path, most free space) | If the path exists on multiple drives use the one with the most free space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| eprand (existing path, random) | Calls **epall** and then randomizes. |
| erofs | Exclusively return **-1** with **errno** set to **EROFS**. By setting **create** functions to this you can in effect turn the filesystem readonly. |
| ff (first found) | Given the order of the drives, as defined at mount time or when configured via xattr interface, act on the first one found. For **create** category it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). |
| lfs (least free space) | Pick the drive with the least available free space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| lus (least used space) | Pick the drive with the least used space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **mfs**. |
| mfs (most free space) | Pick the drive with the most available free space. For **create** category it will exclude readonly drives and those with free space less than **minfreespace**. Falls back to **ff**. |
| newest (newest file) | Pick the file / directory with the largest mtime. For **create** category it will exclude readonly drives and those with free space less than **minfreespace** (unless there is no other option). |
| rand (random) | Calls **all** and then randomizes. |

**epff**, **eplfs**, **eplus**, and **epmf** are path preserving policies. As the descriptions above explain they will only consider drives where the path being accessed exists. Non-path preserving policies will clone paths as necessary.

#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | all    |
| create   | epmfs  |
| search   | ff     |

#### rename & link ####

[rename](http://man7.org/linux/man-pages/man2/rename.2.html) is a tricky function in a merged system. Normally if a rename can't be done atomically due to the source and destination paths existing on different mount points it will return **-1** with **errno = EXDEV**. The atomic rename is most critical for replacing files in place atomically (such as securing writing to a temp file and then replacing a target). The problem is that by merging multiple paths you can have N instances of the source and destinations on different drives. This can lead to several undesirable situtations with or without errors and it's not entirely obvious what to do when an error occurs.

Originally mergerfs would return EXDEV whenever a rename was requested which was cross directory in any way. This made the code simple and was technically complient with POSIX requirements. However, many applications fail to handle EXDEV at all and treat it as a normal error or they only partially support EXDEV (don't respond the same as `mv` would). Such apps include: gvfsd-fuse v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+, NZBGet, Samba's recycling bin feature.

* If using a **create** policy which tries to preserve directory paths (epff,eplfs,eplus,epmfs)
  * Using the **rename** policy get the list of files to rename
  * For each file attempt rename:
    * If failure with ENOENT run **create** policy
    * If create policy returns the same drive as currently evaluating then clone the path
    * Re-attempt rename
  * If **any** of the renames succeed the higher level rename is considered a success
  * If **no** renames succeed the first error encountered will be returned
  * On success:
    * Remove the target from all drives with no source file
    * Remove the source from all drives which failed to rename
* If using a **create** policy which does **not** try to preserve directory paths
  * Using the **rename** policy get the list of files to rename
  * Using the **getattr** policy get the target path
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

The above behavior will help minimize the likelihood of EXDEV being returned but it will still be possible. To remove the possibility all together mergerfs would need to perform the as **mv** does when it receives EXDEV normally.

**link** uses the same basic strategy.

#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is different from all other filesystem functions. While it could have it's own set of policies to tweak its behavior at this time it provides a simple union of files and directories found. Remember that any action or information queried about these files and directories come from the respective function. For instance: an **ls** is a **readdir** and for each file/directory returned **getattr** is called. Meaning the policy of **getattr** is responsible for choosing the file/directory which is the source of the metadata you see in an **ls**.

#### statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple sources on the same drive will not result in double counting it's space.

# BUILDING

**NOTE:** Prebuilt packages can be found at: https://github.com/trapexit/mergerfs/releases

First get the code from [github](http://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.tar.gz
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

Even if xattrs are disabled for mergerfs the [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls against this pseudo file will still work.

Any changes made at runtime are **not** persisted. If you wish for values to persist they must be included as options wherever you configure the mounting of mergerfs (fstab).

##### Keys #####

Use `xattr -l /mount/point/.mergerfs` to see all supported keys. Some are informational and therefore readonly.

###### user.mergerfs.srcmounts ######

Used to query or modify the list of source mounts. When modifying there are several shortcuts to easy manipulation of the list.

| Value        | Description |
|--------------|-------------|
| [list]       | set         |
| +<[list]     | prepend     |
| +>[list]     | append      |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |

###### minfreespace ######

Input: interger with an optional multiplier suffix. **K**, **M**, or **G**.

Output: value in bytes

###### moveonenospc ######

Input: **true** and **false**

Ouput: **true** or **false**

###### categories / funcs ######

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

#### file / directory xattrs ####

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

* https://github.com/trapexit/mergerfs-tools
  * mergerfs.ctl: A tool to make it easier to query and configure mergerfs at runtime
  * mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
  * mergerfs.dedup: Will help identify and optionally remove duplicate files
  * mergerfs.mktrash: Creates FreeDesktop.org Trash specification compatible directories on a mergerfs mount
* https://github.com/trapexit/scorch
  * scorch: A tool to help discover silent corruption of files

# TIPS / NOTES

* https://github.com/trapexit/backup-and-recovery-howtos : A set of guides / howtos on creating a data storage system, backing it up, maintaining it, and recovering from failure.
* If you don't see some directories / files you expect in a merged point be sure the user has permission to all the underlying directories. If `/drive0/a` has is owned by `root:root` with ACLs set to `0700` and `/drive1/a` is `root:root` and `0755` you'll see only `/drive1/a`. Use `mergerfs.fsck` to audit the drive for out of sync permissions.
* Do *not* use `direct_io` if you expect applications (such as rtorrent) to [mmap](http://linux.die.net/man/2/mmap) files. It is not currently supported in FUSE w/ `direct_io` enabled.
* Since POSIX gives you only error or success on calls its difficult to determine the proper behavior when applying the behavior to multiple targets. **mergerfs** will return an error only if all attempts of an action fail. Any success will lead to a success returned.
* The recommended options are **defaults,allow_other**. The **allow_other** is to allow users who are not the one which executed mergerfs access to the mountpoint. **defaults** is described above and should offer the best performance. It's possible that if you're running on an older platform the **splice** features aren't available and could error. In that case simply use the other options manually.
* If write performance is valued more than read it may be useful to enable **direct_io**. Best to benchmark with and without and choose appropriately.
* Remember: some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* An example: [Kodi](http://kodi.tv) and [Plex](http://plex.tv) can use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether to scan for new content rather than simply performing a full scan. If using the current default **getattr** policy of **ff** its possible **Kodi** will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Due to previously mentioned issues its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync).

# KNOWN ISSUES / BUGS

#### rtorrent fails with ENODEV (No such device)

Be sure to turn off `direct_io`. rtorrent and some other applications use [mmap](http://linux.die.net/man/2/mmap) to read and write to files and offer no failback to traditional methods. FUSE does not currently support mmap while using `direct_io`. There will be a performance penalty on writes with `direct_io` off but it's the only way to get such applications to work. If the performance loss is too high for other apps you can mount mergerfs twice. Once with `direct_io` enabled and one without it.

#### mmap performance is really bad

There [is a bug](https://lkml.org/lkml/2016/3/16/260) in caching which affects overall performance of mmap through FUSE in Linux 4.x kernels. It is fixed in [4.4.10 and 4.5.4](https://lkml.org/lkml/2016/5/11/59). 

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

#### mergerfs under heavy load and memory preasure leads to kernel panic

https://lkml.org/lkml/2016/9/14/527

```
[25192.515454] kernel BUG at /build/linux-a2WvEb/linux-4.4.0/mm/workingset.c:346!
[25192.517521] invalid opcode: 0000 [#1] SMP
[25192.519602] Modules linked in: netconsole ip6t_REJECT nf_reject_ipv6 ipt_REJECT nf_reject_ipv4 configfs binfmt_misc veth bridge stp llc nf_conntrack_ipv6 nf_defrag_ipv6 xt_conntrack ip6table_filter ip6_tables xt_multiport iptable_filter ipt_MASQUERADE nf_nat_masquerade_ipv4 xt_comment xt_nat iptable_nat nf_conntrack_ipv4 nf_defrag_ipv4 nf_nat_ipv4 nf_nat nf_conntrack xt_CHECKSUM xt_tcpudp iptable_mangle ip_tables x_tables intel_rapl x86_pkg_temp_thermal intel_powerclamp eeepc_wmi asus_wmi coretemp sparse_keymap kvm_intel ppdev kvm irqbypass mei_me 8250_fintek input_leds serio_raw parport_pc tpm_infineon mei shpchp mac_hid parport lpc_ich autofs4 drbg ansi_cprng dm_crypt algif_skcipher af_alg btrfs raid456 async_raid6_recov async_memcpy async_pq async_xor async_tx xor raid6_pq libcrc32c raid0 multipath linear raid10 raid1 i915 crct10dif_pclmul crc32_pclmul aesni_intel i2c_algo_bit aes_x86_64 drm_kms_helper lrw gf128mul glue_helper ablk_helper syscopyarea cryptd sysfillrect sysimgblt fb_sys_fops drm ahci r8169 libahci mii wmi fjes video [last unloaded: netconsole]
[25192.540910] CPU: 2 PID: 63 Comm: kswapd0 Not tainted 4.4.0-36-generic #55-Ubuntu
[25192.543411] Hardware name: System manufacturer System Product Name/P8H67-M PRO, BIOS 3904 04/27/2013
[25192.545840] task: ffff88040cae6040 ti: ffff880407488000 task.ti: ffff880407488000
[25192.548277] RIP: 0010:[<ffffffff811ba501>]  [<ffffffff811ba501>] shadow_lru_isolate+0x181/0x190
[25192.550706] RSP: 0018:ffff88040748bbe0  EFLAGS: 00010002
[25192.553127] RAX: 0000000000001c81 RBX: ffff8802f91ee928 RCX: ffff8802f91eeb38
[25192.555544] RDX: ffff8802f91ee938 RSI: ffff8802f91ee928 RDI: ffff8804099ba2c0
[25192.557914] RBP: ffff88040748bc08 R08: 000000000001a7b6 R09: 000000000000003f
[25192.560237] R10: 000000000001a750 R11: 0000000000000000 R12: ffff8804099ba2c0
[25192.562512] R13: ffff8803157e9680 R14: ffff8803157e9668 R15: ffff8804099ba2c8
[25192.564724] FS:  0000000000000000(0000) GS:ffff88041f280000(0000) knlGS:0000000000000000
[25192.566990] CS:  0010 DS: 0000 ES: 0000 CR0: 0000000080050033
[25192.569201] CR2: 00007ffabb690000 CR3: 0000000001e0a000 CR4: 00000000000406e0
[25192.571419] Stack:
[25192.573550]  ffff8804099ba2c0 ffff88039e4f86f0 ffff8802f91ee928 ffff8804099ba2c8
[25192.575695]  ffff88040748bd08 ffff88040748bc58 ffffffff811b99bf 0000000000000052
[25192.577814]  0000000000000000 ffffffff811ba380 000000000000008a 0000000000000080
[25192.579947] Call Trace:
[25192.582022]  [<ffffffff811b99bf>] __list_lru_walk_one.isra.3+0x8f/0x130
[25192.584137]  [<ffffffff811ba380>] ? memcg_drain_all_list_lrus+0x190/0x190
[25192.586165]  [<ffffffff811b9a83>] list_lru_walk_one+0x23/0x30
[25192.588145]  [<ffffffff811ba544>] scan_shadow_nodes+0x34/0x50
[25192.590074]  [<ffffffff811a0e9d>] shrink_slab.part.40+0x1ed/0x3d0
[25192.591985]  [<ffffffff811a53da>] shrink_zone+0x2ca/0x2e0
[25192.593863]  [<ffffffff811a64ce>] kswapd+0x51e/0x990
[25192.595737]  [<ffffffff811a5fb0>] ? mem_cgroup_shrink_node_zone+0x1c0/0x1c0
[25192.597613]  [<ffffffff810a0808>] kthread+0xd8/0xf0
[25192.599495]  [<ffffffff810a0730>] ? kthread_create_on_node+0x1e0/0x1e0
[25192.601335]  [<ffffffff8182e34f>] ret_from_fork+0x3f/0x70
[25192.603193]  [<ffffffff810a0730>] ? kthread_create_on_node+0x1e0/0x1e0
```

There is a bug in the kernel. A work around appears to be turning off `splice`. Add `no_splice_write,no_splice_move,no_splice_read` to mergerfs' options. Should be placed after `defaults` if it is used since it will turn them on.

# FAQ

#### Why use mergerfs over mhddfs?

mhddfs is no longer maintained and has some known stability and security issues (see below). MergerFS provides a superset of mhddfs' features and should offer the same or maybe better performance.

#### Why use mergerfs over aufs?

While aufs can offer better peak performance mergerfs offers more configurability and is generally easier to use. mergerfs however doesn't offer the same overlay features (which tends to result in whiteout files being left around the underlying filesystems.)

#### Why use mergerfs over LVM/ZFS/BTRFS/RAID0 drive concatenation / striping?

With simple JBOD / drive concatenation / stripping / RAID0 a single drive failure will lead to full pool failure. mergerfs performs a similar behavior without the catastrophic failure and general lack of recovery. Drives can fail and all other data will continue to be accessable.

When combined with something like [SnapRaid](http://www.snapraid.it) and/or an offsite full backup solution you can have the flexibilty of JBOD without the single point of failure.

#### Can drives be written to directly? Outside of mergerfs while pooled?

Yes. It will be represented immediately in the pool as the policies would describe.

#### Why do I get an "out of space" error even though the system says there's lots of space left?

Please reread the sections above about policies, path preserving, and the **moveonenospc** option. If the policy is path preserving and a drive is almost full and the drive the policy would pick then the writing of the file may fill the drive and receive ENOSPC errors. That is expected with those settings. If you don't want that: enable **moveonenospc** and don't use a path preserving policy.


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

# LINKS

* http://github.com/trapexit/mergerfs
* http://github.com/trapexit/mergerfs-tools
* http://github.com/trapexit/scorch
* http://github.com/trapexit/backup-and-recovery-howtos

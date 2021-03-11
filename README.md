% mergerfs(1) mergerfs user manual
% Antonio SJ Musumeci <trapexit@spawn.link>
% 2021-02-08

# NAME

mergerfs - a featureful union filesystem


# SYNOPSIS

mergerfs -o&lt;options&gt; &lt;branches&gt; &lt;mountpoint&gt;


# DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifying storage and management of files across numerous commodity storage devices. It is similar to **mhddfs**, **unionfs**, and **aufs**.


# FEATURES

* Runs in userspace (FUSE)
* Configurable behaviors / file placement
* Support for extended attributes (xattrs)
* Support for file attributes (chattr)
* Runtime configurable (via xattrs)
* Safe to run as root
* Opportunistic credential caching
* Works with heterogeneous filesystem types
* Handling of writes to full drives (transparently move file to drive with capacity)
* Handles pool of read-only and read/write drives
* Can turn read-only files into symlinks to underlying file
* Hard link copy-on-write / CoW
* Support for POSIX ACLs


# HOW IT WORKS

mergerfs logically merges multiple paths together. Think a union of sets. The file/s or directory/s acted on or presented through mergerfs are based on the policy chosen for that particular action. Read more about policies below.

```
A         +      B        =       C
/disk1           /disk2           /merged
|                |                |
+-- /dir1        +-- /dir1        +-- /dir1
|   |            |   |            |   |
|   +-- file1    |   +-- file2    |   +-- file1
|                |   +-- file3    |   +-- file2
+-- /dir2        |                |   +-- file3
|   |            +-- /dir3        |
|   +-- file4        |            +-- /dir2
|                     +-- file5   |   |
+-- file6                         |   +-- file4
                                  |
                                  +-- /dir3
                                  |   |
                                  |   +-- file5
                                  |
                                  +-- file6
```

mergerfs does **NOT** support the copy-on-write (CoW) or whiteout behaviors found in **aufs** and **overlayfs**. You can **not** mount a read-only filesystem and write to it. However, mergerfs will ignore read-only drives when creating new files so you can mix read-write and read-only drives. It also does **NOT** split data across drives. It is not RAID0 / striping. It is simply a union of other filesystems.


# TERMINOLOGY

* branch: A base path used in the pool.
* pool: The mergerfs mount. The union of the branches.
* relative path: The path in the pool relative to the branch and mount.
* function: A filesystem call (open, unlink, create, getattr, rmdir, etc.)
* category: A collection of functions based on basic behavior (action, create, search).
* policy: The algorithm used to select a file when performing a function.
* path preservation: Aspect of some policies which includes checking the path for which a file would be created.


# BASIC SETUP

If you don't already know that you have a special use case then just start with one of the following option sets.

#### You need `mmap` (used by rtorrent and many sqlite3 base software)

`allow_other,use_ino,cache.files=partial,dropcacheonclose=true,category.create=mfs`

#### You don't need `mmap`

`allow_other,use_ino,cache.files=off,dropcacheonclose=true,category.create=mfs`


See the mergerfs [wiki for real world deployments](https://github.com/trapexit/mergerfs/wiki/Real-World-Deployments) for comparisons / ideas.


# OPTIONS

These options are the same regardless you use them with the `mergerfs` commandline program, used in fstab, or in a config file.

### mount options

* **config**: Path to a config file. Same arguments as below in key=val / ini style format.
* **branches**: Colon delimited list of branches.
* **allow_other**: A libfuse option which allows users besides the one which ran mergerfs to see the filesystem. This is required for most use-cases.
* **minfreespace=SIZE**: The minimum space value used for creation policies. Can be overridden by branch specific option. Understands 'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte respectively. (default: 4G)
* **moveonenospc=BOOL|POLICY**: When enabled if a **write** fails with **ENOSPC** (no space left on device) or **EDQUOT** (disk quota exceeded) the policy selected will run to find a new location for the file. An attempt to move the file to that branch will occur (keeping all metadata possible) and if successful the original is unlinked and the write retried. (default: false, true = mfs)
* **use_ino**: Causes mergerfs to supply file/directory inodes rather than libfuse. While not a default it is recommended it be enabled so that linked files share the same inode value.
* **inodecalc=passthrough|path-hash|devino-hash|hybrid-hash**: Selects the inode calculation algorithm. (default: hybrid-hash)
* **dropcacheonclose=BOOL**: When a file is requested to be closed call `posix_fadvise` on it first to instruct the kernel that we no longer need the data and it can drop its cache. Recommended when **cache.files=partial|full|auto-full** to limit double caching. (default: false)
* **symlinkify=BOOL**: When enabled and a file is not writable and its mtime or ctime is older than **symlinkify_timeout** files will be reported as symlinks to the original files. Please read more below before using. (default: false)
* **symlinkify_timeout=UINT**: Time to wait, in seconds, to activate the **symlinkify** behavior. (default: 3600)
* **nullrw=BOOL**: Turns reads and writes into no-ops. The request will succeed but do nothing. Useful for benchmarking mergerfs. (default: false)
* **ignorepponrename=BOOL**: Ignore path preserving on rename. Typically rename and link act differently depending on the policy of `create` (read below). Enabling this will cause rename and link to always use the non-path preserving behavior. This means files, when renamed or linked, will stay on the same drive. (default: false)
* **security_capability=BOOL**: If false return ENOATTR when xattr security.capability is queried. (default: true)
* **xattr=passthrough|noattr|nosys**: Runtime control of xattrs. Default is to passthrough xattr requests. 'noattr' will short circuit as if nothing exists. 'nosys' will respond with ENOSYS as if xattrs are not supported or disabled. (default: passthrough)
* **link_cow=BOOL**: When enabled if a regular file is opened which has a link count > 1 it will copy the file to a temporary file and rename over the original. Breaking the link and providing a basic copy-on-write function similar to cow-shell. (default: false)
* **statfs=base|full**: Controls how statfs works. 'base' means it will always use all branches in statfs calculations. 'full' is in effect path preserving and only includes drives where the path exists. (default: base)
* **statfs_ignore=none|ro|nc**: 'ro' will cause statfs calculations to ignore available space for branches mounted or tagged as 'read-only' or 'no create'. 'nc' will ignore available space for branches tagged as 'no create'. (default: none)
* **nfsopenhack=off|git|all**: A workaround for exporting mergerfs over NFS where there are issues with creating files for write while setting the mode to read-only. (default: off)
* **follow-symlinks=never|directory|regular|all**: Turns symlinks into what they point to. (default: never)
* **link-exdev=passthrough|rel-symlink|abs-base-symlink|abs-pool-symlink**: When a link fails with EXDEV optionally create a symlink to the file instead.
* **rename-exdev=passthrough|rel-symlink|abs-symlink**: When a rename fails with EXDEV optionally move the file to a special directory and symlink to it.
* **posix_acl=BOOL**: Enable POSIX ACL support (if supported by kernel and underlying filesystem). (default: false)
* **async_read=BOOL**: Perform reads asynchronously. If disabled or unavailable the kernel will ensure there is at most one pending read request per file handle and will attempt to order requests by offset. (default: true)
* **fuse_msg_size=UINT**: Set the max number of pages per FUSE message. Only available on Linux >= 4.20 and ignored otherwise. (min: 1; max: 256; default: 256)
* **threads=INT**: Number of threads to use in multithreaded mode. When set to zero it will attempt to discover and use the number of logical cores. If the lookup fails it will fall back to using 4. If the thread count is set negative it will look up the number of cores then divide by the absolute value. ie. threads=-2 on an 8 core machine will result in 8 / 2 = 4 threads. There will always be at least 1 thread. NOTE: higher number of threads increases parallelism but usually decreases throughput. (default: 0)
* **fsname=STR**: Sets the name of the filesystem as seen in **mount**, **df**, etc. Defaults to a list of the source paths concatenated together with the longest common prefix removed.
* **func.FUNC=POLICY**: Sets the specific FUSE function's policy. See below for the list of value types. Example: **func.getattr=newest**
* **category.action=POLICY**: Sets policy of all FUSE functions in the action category. (default: epall)
* **category.create=POLICY**: Sets policy of all FUSE functions in the create category. (default: epmfs)
* **category.search=POLICY**: Sets policy of all FUSE functions in the search category. (default: ff)
* **cache.open=UINT**: 'open' policy cache timeout in seconds. (default: 0)
* **cache.statfs=UINT**: 'statfs' cache timeout in seconds. (default: 0)
* **cache.attr=UINT**: File attribute cache timeout in seconds. (default: 1)
* **cache.entry=UINT**: File name lookup cache timeout in seconds. (default: 1)
* **cache.negative_entry=UINT**: Negative file name lookup cache timeout in seconds. (default: 0)
* **cache.files=libfuse|off|partial|full|auto-full**: File page caching mode (default: libfuse)
* **cache.writeback=BOOL**: Enable kernel writeback caching (default: false)
* **cache.symlinks=BOOL**: Cache symlinks (if supported by kernel) (default: false)
* **cache.readdir=BOOL**: Cache readdir (if supported by kernel) (default: false)
* **direct_io**: deprecated - Bypass page cache. Use `cache.files=off` instead. (default: false)
* **kernel_cache**: deprecated - Do not invalidate data cache on file open. Use `cache.files=full` instead. (default: false)
* **auto_cache**: deprecated - Invalidate data cache if file mtime or size change. Use `cache.files=auto-full` instead. (default: false)
* **async_read**: deprecated - Perform reads asynchronously. Use `async_read=true` instead.
* **sync_read**: deprecated - Perform reads synchronously. Use `async_read=false` instead.


**NOTE:** Options are evaluated in the order listed so if the options are **func.rmdir=rand,category.action=ff** the **action** category setting will override the **rmdir** setting.


#### Value Types

* BOOL = 'true' | 'false'
* INT = [MIN_INT,MAX_INT]
* UINT = [0,MAX_INT]
* SIZE = 'NNM'; NN = INT, M = 'K' | 'M' | 'G' | 'T'
* STR = string
* FUNC = filesystem function
* CATEGORY = function category
* POLICY = mergerfs function policy


### branches

The 'branches' argument is a colon (':') delimited list of paths to be pooled together. It does not matter if the paths are on the same or different drives nor does it matter the filesystem (within reason). Used and available space will not be duplicated for paths on the same device and any features which aren't supported by the underlying filesystem (such as file attributes or extended attributes) will return the appropriate errors.

Branches currently have two options which can be set. A type which impacts whether or not the branch is included in a policy calculation and a individual minfreespace value. The values are set by prepending an `=` at the end of a branch designation and using commas as delimiters. Example: /mnt/drive=RW,1234


#### branch type

* RW: (read/write) - Default behavior. Will be eligible in all policy categories.
* RO: (read-only) - Will be excluded from `create` and `action` policies. Same as a read-only mounted filesystem would be (though faster to process).
* NC: (no-create) - Will be excluded from `create` policies. You can't create on that branch but you can change or delete.


#### minfreespace

Same purpose as the global option but specific to the branch. If not set the global value is used.


#### globbing

To make it easier to include multiple branches mergerfs supports [globbing](http://linux.die.net/man/7/glob). **The globbing tokens MUST be escaped when using via the shell else the shell itself will apply the glob itself.**


```
# mergerfs -o allow_other,use_ino /mnt/disk\*:/mnt/cdrom /media/drives
```

The above line will use all mount points in /mnt prefixed with **disk** and the **cdrom**.

To have the pool mounted at boot or otherwise accessible from related tools use **/etc/fstab**.

```
# <file system>        <mount point>  <type>         <options>             <dump>  <pass>
/mnt/disk*:/mnt/cdrom  /mnt/pool      fuse.mergerfs  allow_other,use_ino   0       0
```

**NOTE:** the globbing is done at mount or when updated using the runtime API. If a new directory is added matching the glob after the fact it will not be automatically included.

**NOTE:** for mounting via **fstab** to work you must have **mount.fuse** installed. For Ubuntu/Debian it is included in the **fuse** package.


### inodecalc

Inodes (st_ino) are unique identifiers within a filesystem. Each mounted filesystem has device ID (st_dev) as well and together they can uniquely identify a file on the whole of the system. Entries on the same device with the same inode are in fact references to the same underlying file. It is a many to one relationship between names and an inode. Directories, however, do not have multiple links on most systems due to the complexity they add.

FUSE allows the server (mergerfs) to set inode values but not device IDs. Creating an inode value is somewhat complex in mergerfs' case as files aren't really in its control. If a policy changes what directory or file is to be selected or something changes out of band it becomes unclear what value should be used. Most software does not to care what the values are but those that do often break if a value changes unexpectedly. The tool `find` will abort a directory walk if it sees a directory inode change. NFS will return stale handle errors if the inode changes out of band. File dedup tools will usually leverage device ids and inodes as a shortcut in searching for duplicate files and would resort to full file comparisons should it find different inode values.

mergerfs offers multiple ways to calculate the inode in hopes of covering different usecases.

* passthrough: Passes through the underlying inode value. Mostly intended for testing as using this does not address any of the problems mentioned above and could confuse file deduplication software as inodes from different filesystems can be the same.
* path-hash: Hashes the relative path of the entry in question. The underlying file's values are completely ignored. This means the inode value will always be the same for that file path. This is useful when using NFS and you make changes out of band such as copy data between branches. This also means that entries that do point to the same file will not be recognizable via inodes. That **does not** mean hard links don't work. They will.
* path-hash32: 32bit version of path-hash.
* devino-hash: Hashes the device id and inode of the underlying entry. This won't prevent issues with NFS should the policy pick a different file or files move out of band but will present the same inode for underlying files that do too.
* devino-hash32: 32bit version of devino-hash.
* hybrid-hash: Performs `path-hash` on directories and `devino-hash` on other file types. Since directories can't have hard links the static value won't make a difference and the files will get values useful for finding duplicates. Probably the best to use if not using NFS. As such it is the default.
* hybrid-hash32: 32bit version of hybrid-hash.

32bit versions are provided as there is some software which does not handle 64bit inodes well.

While there is a risk of hash collision in tests of a couple million entries there were zero collisions. Unlike a typical filesystem FUSE filesystems can reuse inodes and not refer to the same entry. The internal identifier used to reference a file in FUSE is different from the inode value presented. The former is the `nodeid` and is actually a tuple of 2 64bit values: `nodeid` and `generation`. This tuple is not client facing. The inode that is presented to the client is passed through the kernel uninterpreted.

From FUSE docs regarding `use_ino`:

```
Honor the st_ino field in the functions getattr() and
fill_dir(). This value is used to fill in the st_ino field
in the stat(2), lstat(2), fstat(2) functions and the d_ino
field in the readdir(2) function. The filesystem does not
have to guarantee uniqueness, however some applications
rely on this value being unique for the whole filesystem.
Note that this does *not* affect the inode that libfuse
and the kernel use internally (also called the "nodeid").
```

In the future the `use_ino` option will probably be removed as this feature should replace the original libfuse inode calculation strategy. Currently you still need to use `use_ino` in order to enable `inodecalc`.


### fuse_msg_size

FUSE applications communicate with the kernel over a special character device: `/dev/fuse`. A large portion of the overhead associated with FUSE is the cost of going back and forth from user space and kernel space over that device. Generally speaking the fewer trips needed the better the performance will be. Reducing the number of trips can be done a number of ways. Kernel level caching and increasing message sizes being two significant ones. When it comes to reads and writes if the message size is doubled the number of trips are approximately halved.

In Linux 4.20 a new feature was added allowing the negotiation of the max message size. Since the size is in multiples of [pages](https://en.wikipedia.org/wiki/Page_(computer_memory)) the feature is called `max_pages`. There is a maximum `max_pages` value of 256 (1MiB) and minimum of 1 (4KiB). The default used by Linux >=4.20, and hardcoded value used before 4.20, is 32 (128KiB). In mergerfs its referred to as `fuse_msg_size` to make it clear what it impacts and provide some abstraction.

Since there should be no downsides to increasing `fuse_msg_size` / `max_pages`, outside a minor bump in RAM usage due to larger message buffers, mergerfs defaults the value to 256. On kernels before 4.20 the value has no effect. The reason the value is configurable is to enable experimentation and benchmarking. See the BENCHMARKING section for examples.


### follow-symlinks

This feature, when enabled, will cause symlinks to be interpreted by mergerfs as their target (depending on the mode).

When there is a getattr/stat request for a file mergerfs will check if the file is a symlink and depending on the `follow-symlinks` setting will replace the information about the symlink with that of that which it points to.

When unlink'ing or rmdir'ing the followed symlink it will remove the symlink itself and not that which it points to.

* never: Behave as normal. Symlinks are treated as such.
* directory: Resolve symlinks only which point to directories.
* regular: Resolve symlinks only which point to regular files.
* all: Resolve all symlinks to that which they point to.

Symlinks which do not point to anything are left as is.

WARNING: This feature works but there might be edge cases yet found. If you find any odd behaviors please file a ticket on [github](https://github.com/trapexit/mergerfs/issues).


### link-exdev

If using path preservation and a `link` fails with EXDEV make a call to `symlink` where the `target` is the `oldlink` and the `linkpath` is the `newpath`. The `target` value is determined by the value of `link-exdev`.

* passthrough: Return EXDEV as normal.
* rel-symlink: A relative path from the `newpath`.
* abs-base-symlink: A absolute value using the underlying branch.
* abs-pool-symlink: A absolute value using the mergerfs mount point.

NOTE: It is possible that some applications check the file they link. In those cases it is possible it will error or complain.


### rename-exdev

If using path preservation and a `rename` fails with EXDEV:

1. Move file from **/branch/a/b/c** to **/branch/.mergerfs_rename_exdev/a/b/c**.
2. symlink the rename's `newpath` to the moved file.

The `target` value is determined by the value of `rename-exdev`.

* passthrough: Return EXDEV as normal.
* rel-symlink: A relative path from the `newpath`.
* abs-symlink: A absolute value using the mergerfs mount point.

NOTE: It is possible that some applications check the file they rename. In those cases it is possible it will error or complain.

NOTE: The reason `abs-symlink` is not split into two like `link-exdev` is due to the complexities in managing absolute base symlinks when multiple `oldpaths` exist.


### symlinkify

Due to the levels of indirection introduced by mergerfs and the underlying technology FUSE there can be varying levels of performance degradation. This feature will turn non-directories which are not writable into symlinks to the original file found by the `readlink` policy after the mtime and ctime are older than the timeout.

**WARNING:** The current implementation has a known issue in which if the file is open and being used when the file is converted to a symlink then the application which has that file open will receive an error when using it. This is unlikely to occur in practice but is something to keep in mind.

**WARNING:** Some backup solutions, such as CrashPlan, do not backup the target of a symlink. If using this feature it will be necessary to point any backup software to the original drives or configure the software to follow symlinks if such an option is available. Alternatively create two mounts. One for backup and one for general consumption.


### nullrw

Due to how FUSE works there is an overhead to all requests made to a FUSE filesystem that wouldn't exist for an in kernel one. Meaning that even a simple passthrough will have some slowdown. However, generally the overhead is minimal in comparison to the cost of the underlying I/O. By disabling the underlying I/O we can test the theoretical performance boundaries.

By enabling `nullrw` mergerfs will work as it always does **except** that all reads and writes will be no-ops. A write will succeed (the size of the write will be returned as if it were successful) but mergerfs does nothing with the data it was given. Similarly a read will return the size requested but won't touch the buffer.

See the BENCHMARKING section for suggestions on how to test.


### xattr

Runtime extended attribute support can be managed via the `xattr` option. By default it will passthrough any xattr calls. Given xattr support is rarely used and can have significant performance implications mergerfs allows it to be disabled at runtime. The performance problems mostly comes when file caching is enabled. The kernel will send a `getxattr` for `security.capability` *before every single write*. It doesn't cache the responses to any `getxattr`. This might be addressed in the future but for now mergerfs can really only offer the following workarounds.

`noattr` will cause mergerfs to short circuit all xattr calls and return ENOATTR where appropriate. mergerfs still gets all the requests but they will not be forwarded on to the underlying filesystems. The runtime control will still function in this mode.

`nosys` will cause mergerfs to return ENOSYS for any xattr call. The difference with `noattr` is that the kernel will cache this fact and itself short circuit future calls. This is more efficient than `noattr` but will cause mergerfs' runtime control via the hidden file to stop working.


### nfsopenhack

NFS is not fully POSIX compliant and historically certain behaviors, such as opening files with O_EXCL, are not or not well supported. When mergerfs (or any FUSE filesystem) is exported over NFS some of these issues come up due to how NFS and FUSE interact.

This hack addresses the issue where the creation of a file with a read-only mode but with a read/write or write only flag. Normally this is perfectly valid but NFS chops the one open call into multiple calls. Exactly how it is translated depends on the configuration and versions of the NFS server and clients but it results in a permission error because a normal user is not allowed to open a read-only file as writable.

Even though it's a more niche situation this hack breaks normal security and behavior and as such is `off` by default. If set to `git` it will only perform the hack when the path in question includes `/.git/`. `all` will result it it applying anytime a readonly file which is empty is opened for writing.


# FUNCTIONS, CATEGORIES and POLICIES

The POSIX filesystem API is made up of a number of functions. **creat**, **stat**, **chown**, etc. For ease of configuration in mergerfs most of the core functions are grouped into 3 categories: **action**, **create**, and **search**. These functions and categories can be assigned a policy which dictates which branch is chosen when performing that function.

Some functions, listed in the category `N/A` below, can not be assigned the normal policies. These functions work with file handles, rather than file paths, which were created by `open` or `create`. That said many times the current FUSE kernel driver will not always provide the file handle when a client calls `fgetattr`, `fchown`, `fchmod`, `futimens`, `ftruncate`, etc. This means it will call the regular, path based, versions. `readdir` has no real need for a policy given the purpose is merely to return a list of entries in a directory. `statfs`'s behavior can be modified via other options.

When using policies which are based on a branch's available space the base path provided is used. Not the full path to the file in question. Meaning that mounts in the branch won't be considered in the space calculations. The reason is that it doesn't really work for non-path preserving policies and can lead to non-obvious behaviors.

NOTE: While any policy can be assigned to a function or category though some may not be very useful in practice. For instance: **rand** (random) may be useful for file creation (create) but could lead to very odd behavior if used for `chmod` if there were more than one copy of the file.


### Functions and their Category classifications

| Category | FUSE Functions                                                                      |
|----------|-------------------------------------------------------------------------------------|
| action   | chmod, chown, link, removexattr, rename, rmdir, setxattr, truncate, unlink, utimens |
| create   | create, mkdir, mknod, symlink                                                       |
| search   | access, getattr, getxattr, ioctl (directories), listxattr, open, readlink           |
| N/A      | fchmod, fchown, futimens, ftruncate, fallocate, fgetattr, fsync, ioctl (files), read, readdir, release, statfs, write, copy_file_range |

In cases where something may be searched for (such as a path to clone) **getattr** will usually be used.


### Policies

A policy is the algorithm used to choose a branch or branches for a function to work on. Think of them as ways to filter and sort branches.

Any function in the `create` category will clone the relative path if needed. Some other functions (`rename`,`link`,`ioctl`) have special requirements or behaviors which you can read more about below.


#### Filtering

Policies basically search branches and create a list of files / paths for functions to work on. The policy is responsible for filtering and sorting the branches. Filters include **minfreespace**, whether or not a branch is mounted read-only, and the branch tagging (RO,NC,RW). These filters are applied across all policies unless otherwise noted.

* No **search** function policies filter.
* All **action** function policies filter out branches which are mounted **read-only** or tagged as **RO (read-only)**.
* All **create** function policies filter out branches which are mounted **read-only**, tagged **RO (read-only)** or **NC (no create)**, or has available space less than `minfreespace`.

Policies may have their own additional filtering such as those that require existing paths to be present.

If all branches are filtered an error will be returned. Typically **EROFS** (read-only filesystem) or **ENOSPC** (no space left on device) depending on the most recent reason for filtering a branch. **ENOENT** will be returned if no elegible branch is found.


#### Path Preservation

Policies, as described below, are of two basic types. `path preserving` and `non-path preserving`.

All policies which start with `ep` (**epff**, **eplfs**, **eplus**, **epmfs**, **eprand**) are `path preserving`. `ep` stands for `existing path`.

A path preserving policy will only consider drives where the relative path being accessed already exists.

When using non-path preserving policies paths will be cloned to target drives as necessary.

With the `msp` or `most shared path` policies they are defined as `path preserving` for the purpose of controlling `link` and `rename`'s behaviors since `ignorepponrename` is available to disable that behavior. In mergerfs v3.0 the path preserving behavior of rename and link will likely be separated from the policy all together.


#### Policy descriptions

A policy's behavior differs, as mentioned above, based on the function it is used with. Sometimes it really might not make sense to even offer certain policies because they are literally the same as others but it makes things a bit more uniform. In mergerfs 3.0 this might change.


| Policy           | Description                                                |
|------------------|------------------------------------------------------------|
| all | Search: Same as **epall**. Action: Same as **epall**. Create: for **mkdir**, **mknod**, and **symlink** it will apply to all branches. **create** works like **ff**. |
| epall (existing path, all) | Search: Same as **epff** (but more expensive because it doesn't stop after finding a valid branch). Action: apply to all found. Create: for **mkdir**, **mknod**, and **symlink** it will apply to all found. **create** works like **epff** (but more expensive because it doesn't stop after finding a valid branch). |
| epff (existing path, first found) | Given the order of the branches, as defined at mount time or configured at runtime, act on the first one found where the relative path exists. |
| eplfs (existing path, least free space) | Of all the branches on which the relative path exists choose the drive with the least free space. |
| eplus (existing path, least used space) | Of all the branches on which the relative path exists choose the drive with the least used space. |
| epmfs (existing path, most free space) | Of all the branches on which the relative path exists choose the drive with the most free space. |
| eppfrd (existing path, percentage free random distribution) | Like **pfrd** but limited to existing paths.  |
| eprand (existing path, random) | Calls **epall** and then randomizes. Returns 1. |
| erofs | Exclusively return **-1** with **errno** set to **EROFS** (read-only filesystem). |
| ff (first found) | Search: Same as **epff**. Action: Same as **epff**. Create: Given the order of the drives, as defined at mount time or configured at runtime, act on the first one found. |
| lfs (least free space) | Search: Same as **eplfs**. Action: Same as **eplfs**. Create: Pick the drive with the least available free space. |
| lus (least used space) | Search: Same as **eplus**. Action: Same as **eplus**. Create: Pick the drive with the least used space. |
| mfs (most free space) | Search: Same as **epmfs**. Action: Same as **epmfs**. Create: Pick the drive with the most available free space. |
| msplfs (most shared path, least free space) | Search: Same as **eplfs**. Action: Same as **eplfs**. Create: like **eplfs** but walk back the path if it fails to find a branch at that level. |
| msplus (most shared path, least used space) | Search: Same as **eplus**. Action: Same as **eplus**. Create: like **eplus** but walk back the path if it fails to find a branch at that level. |
| mspmfs (most shared path, most free space) | Search: Same as **epmfs**. Action: Same as **epmfs**. Create: like **epmfs** but walk back the path if it fails to find a branch at that level. |
| msppfrd (most shared path, percentage free random distribution) | Search: Same as **eppfrd**. Action: Same as **eppfrd**. Create: Like **eppfrd** but will walk back the path if it fails to find a branch at that level. |
| newest | Pick the file / directory with the largest mtime. |
| pfrd (percentage free random distribution) | Search: Same as **eppfrd**. Action: Same as **eppfrd**. Create: Chooses a branch at random with the likelihood of selection based on a branch's available space relative to the total. |
| rand (random) | Calls **all** and then randomizes. Returns 1. |

**NOTE:** If you are using an underlying filesystem that reserves blocks such as ext2, ext3, or ext4 be aware that mergerfs respects the reservation by using `f_bavail` (number of free blocks for unprivileged users) rather than `f_bfree` (number of free blocks) in policy calculations. **df** does NOT use `f_bavail`, it uses `f_bfree`, so direct comparisons between **df** output and mergerfs' policies is not appropriate.


#### Defaults ####

| Category | Policy |
|----------|--------|
| action   | epall  |
| create   | epmfs  |
| search   | ff     |


#### ioctl

When `ioctl` is used with an open file then it will use the file handle which was created at the original `open` call. However, when using `ioctl` with a directory mergerfs will use the `open` policy to find the directory to act on.


#### rename & link ####

**NOTE:** If you're receiving errors from software when files are moved / renamed / linked then you should consider changing the create policy to one which is **not** path preserving, enabling `ignorepponrename`, or contacting the author of the offending software and requesting that `EXDEV` (cross device / improper link) be properly handled.

`rename` and `link` are tricky functions in a union filesystem. `rename` only works within a single filesystem or device. If a rename can't be done atomically due to the source and destination paths existing on different mount points it will return **-1** with **errno = EXDEV** (cross device / improper link). So if a `rename`'s source and target are on different drives within the pool it creates an issue.

Originally mergerfs would return EXDEV whenever a rename was requested which was cross directory in any way. This made the code simple and was technically compliant with POSIX requirements. However, many applications fail to handle EXDEV at all and treat it as a normal error or otherwise handle it poorly. Such apps include: gvfsd-fuse v1.20.3 and prior, Finder / CIFS/SMB client in Apple OSX 10.9+, NZBGet, Samba's recycling bin feature.

As a result a compromise was made in order to get most software to work while still obeying mergerfs' policies. Below is the basic logic.

* If using a **create** policy which tries to preserve directory paths (epff,eplfs,eplus,epmfs)
  * Using the **rename** policy get the list of files to rename
  * For each file attempt rename:
    * If failure with ENOENT (no such file or directory) run **create** policy
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

The above behavior will help minimize the likelihood of EXDEV being returned but it will still be possible.

**link** uses the same strategy but without the removals.


#### readdir ####

[readdir](http://linux.die.net/man/3/readdir) is different from all other filesystem functions. While it could have its own set of policies to tweak its behavior at this time it provides a simple union of files and directories found. Remember that any action or information queried about these files and directories come from the respective function. For instance: an **ls** is a **readdir** and for each file/directory returned **getattr** is called. Meaning the policy of **getattr** is responsible for choosing the file/directory which is the source of the metadata you see in an **ls**.


#### statfs / statvfs ####

[statvfs](http://linux.die.net/man/2/statvfs) normalizes the source drives based on the fragment size and sums the number of adjusted blocks and inodes. This means you will see the combined space of all sources. Total, used, and free. The sources however are dedupped based on the drive so multiple sources on the same drive will not result in double counting its space. Filesystems mounted further down the tree of the branch will not be included when checking the mount's stats.

The options `statfs` and `statfs_ignore` can be used to modify `statfs` behavior.


# ERROR HANDLING

POSIX filesystem functions offer a single return code meaning that there is some complication regarding the handling of multiple branches as mergerfs does. It tries to handle errors in a way that would generally return meaningful values for that particular function.

### chmod, chown, removexattr, setxattr, truncate, utimens

1) if no error: return 0 (success)
2) if no successes: return first error
3) if one of the files acted on was the same as the related search function: return its value
4) return 0 (success)

While doing this increases the complexity and cost of error handling, particularly step 3, this provides probably the most reasonable return value.


### unlink, rmdir

1) if no errors: return 0 (success)
2) return first error

Older version of mergerfs would return success if any success occurred but for unlink and rmdir there are downstream assumptions that, while not impossible to occur, can confuse some software.


### others

For search functions there is always a single thing acted on and as such whatever return value that comes from the single function call is returned.

For create functions `mkdir`, `mknod`, and `symlink` which don't return a file descriptor and therefore can have `all` or `epall` policies it will return success if any of the calls succeed and an error otherwise.


# BUILD / UPDATE

**NOTE:** Prebuilt packages can be found at and recommended for most users: https://github.com/trapexit/mergerfs/releases
**NOTE:** Only tagged releases are supported. `master` and other branches should be considered works in progress.

First get the code from [github](https://github.com/trapexit/mergerfs).

```
$ git clone https://github.com/trapexit/mergerfs.git
$ # or
$ wget https://github.com/trapexit/mergerfs/releases/download/<ver>/mergerfs-<ver>.tar.gz
```

#### Debian / Ubuntu
```
$ cd mergerfs
$ sudo tools/install-build-pkgs
$ make deb
$ sudo dpkg -i ../mergerfs_<version>_<arch>.deb
```

#### RHEL / CentOS /Fedora
```
$ su -
# cd mergerfs
# tools/install-build-pkgs
# make rpm
# rpm -i rpmbuild/RPMS/<arch>/mergerfs-<version>.<arch>.rpm
```

#### Generically

Have git, g++, make, python installed.

```
$ cd mergerfs
$ make
$ sudo make install
```

#### Build options

```
$ make help
usage: make

make USE_XATTR=0      - build program without xattrs functionality
make STATIC=1         - build static binary
make LTO=1            - build with link time optimization
```


# UPGRADE

mergerfs can be upgraded live by mounting on top of the previous instance. Simply install the new version of mergerfs and follow the instructions below.

Add `nonempty` to your mergerfs option list and call mergerfs again or if using `/etc/fstab` call for it to mount again. Existing open files and such will continue to work fine though they won't see runtime changes since any such change would be the new mount. If you plan on changing settings with the new mount you should / could apply those before mounting the new version.

```
$ sudo mount /mnt/mergerfs
$ mount | grep mergerfs
media on /mnt/mergerfs type fuse.mergerfs (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
media on /mnt/mergerfs type fuse.mergerfs (rw,relatime,user_id=0,group_id=0,default_permissions,allow_other)
```

A problem with this approach is that the underlying instance will continue to run even if the software using it stop or are restarted. To work around this you can use a "lazy umount". Before mounting over top the mount point with the new instance of mergerfs issue: `umount -l <mergerfs_mountpoint>`.



# RUNTIME CONFIG

#### ioctl

The original runtime config API was via xattr calls. This however became an issue when needing to disable xattr. While slightly less convenient ioctl does not have the same problems and will be the main API going forward.

The keys are the same as the command line option arguments as well as the config file.

##### requests / commands

All commands take a 4096 byte char buffer.

* read keys: get a nul '\0' delimited list of option keys
  * _IOWR(0xDF,0,char[4096]) = 0xD000DF00
  * on success ioctl return value is the total length
* read value: get an option value
  * _IOWR(0xDF,1,char[4096]) = 0xD000DF01
  * the key is passed in via the char buffer as a nul '\0' terminated string
  * on success ioctl return value is the total length
* write value: set an option value
  * _IOW(0xDF,2,char[4096]) = 0x5000DF02
  * the key and value is passed in via the char buffer as a nul '\0' terminated string in the format of `key=value`
  * on success ioctl return value is 0
* file info: get mergerfs metadata info for a file
  * _IOWR(0xDF,3,char[4096]) = 0xD000DF03
  * the key is passed in via the char buffer as a nul '\0' terminated string
  * on success the ioctl return value is the total length
  * keys:
    * basepath: the base mount point for the file according to the getattr policy
    * relpath: the relative path of the file from the mount point
    * fullpath: the full path of the underlying file according to the getattr policy
    * allpaths: a NUL '\0' delimited list of full paths to all files found


#### .mergerfs pseudo file (deprecated) ####

NOTE: this interface will be removed in mergerfs 3.0

```
<mountpoint>/.mergerfs
```

There is a pseudo file available at the mount point which allows for the runtime modification of certain **mergerfs** options. The file will not show up in **readdir** but can be **stat**'ed and manipulated via [{list,get,set}xattrs](http://linux.die.net/man/2/listxattr) calls.

Any changes made at runtime are **not** persisted. If you wish for values to persist they must be included as options wherever you configure the mounting of mergerfs (/etc/fstab).


##### Keys #####

Use `xattr -l /mountpoint/.mergerfs` to see all supported keys. Some are informational and therefore read-only. `setxattr` will return EINVAL (invalid argument) on read-only keys.


##### Values #####

Same as the command line.


###### user.mergerfs.branches ######

**NOTE:** formerly `user.mergerfs.srcmounts` but said key is still supported.

Used to query or modify the list of branches. When modifying there are several shortcuts to easy manipulation of the list.

| Value        | Description |
|--------------|-------------|
| [list]       | set         |
| +<[list]     | prepend     |
| +>[list]     | append      |
| -[list]      | remove all values provided |
| -<           | remove first in list |
| ->           | remove last in list |

`xattr -w user.mergerfs.branches +</mnt/drive3 /mnt/pool/.mergerfs`

The `=NC`, `=RO`, `=RW` syntax works just as on the command line.


##### Example #####

```
[trapexit:/mnt/mergerfs] $ xattr -l .mergerfs
user.mergerfs.branches: /mnt/a=RW:/mnt/b=RW
user.mergerfs.minfreespace: 4294967295
user.mergerfs.moveonenospc: false
...

[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.category.search .mergerfs
ff

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.category.search newest .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.category.search .mergerfs
newest

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches +/mnt/c .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/a:/mnt/b:/mnt/c

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches =/mnt/c .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/c

[trapexit:/mnt/mergerfs] $ xattr -w user.mergerfs.branches '+</mnt/a:/mnt/b' .mergerfs
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.branches .mergerfs
/mnt/a:/mnt/b:/mnt/c
```


#### file / directory xattrs ####

While they won't show up when using [listxattr](http://linux.die.net/man/2/listxattr) **mergerfs** offers a number of special xattrs to query information about the files served. To access the values you will need to issue a [getxattr](http://linux.die.net/man/2/getxattr) for one of the following:

* **user.mergerfs.basepath**: the base mount point for the file given the current getattr policy
* **user.mergerfs.relpath**: the relative path of the file from the perspective of the mount point
* **user.mergerfs.fullpath**: the full path of the original file given the getattr policy
* **user.mergerfs.allpaths**: a NUL ('\0') separated list of full paths to all files found

```
[trapexit:/mnt/mergerfs] $ ls
A B C
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.fullpath A
/mnt/a/full/path/to/A
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.basepath A
/mnt/a
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.relpath A
/full/path/to/A
[trapexit:/mnt/mergerfs] $ xattr -p user.mergerfs.allpaths A | tr '\0' '\n'
/mnt/a/full/path/to/A
/mnt/b/full/path/to/A
```


# TOOLING

* https://github.com/trapexit/mergerfs-tools
  * mergerfs.ctl: A tool to make it easier to query and configure mergerfs at runtime
  * mergerfs.fsck: Provides permissions and ownership auditing and the ability to fix them
  * mergerfs.dedup: Will help identify and optionally remove duplicate files
  * mergerfs.dup: Ensure there are at least N copies of a file across the pool
  * mergerfs.balance: Rebalance files across drives by moving them from the most filled to the least filled
  * mergerfs.consolidate: move files within a single mergerfs directory to the drive with most free space
* https://github.com/trapexit/scorch
  * scorch: A tool to help discover silent corruption of files and keep track of files
* https://github.com/trapexit/bbf
  * bbf (bad block finder): a tool to scan for and 'fix' hard drive bad blocks and find the files using those blocks


# CACHING

#### page caching

https://en.wikipedia.org/wiki/Page_cache

tl;dr:
* cache.files=off: Disables page caching. Underlying files cached, mergerfs files are not.
* cache.files=partial: Enables page caching. Underlying files cached, mergerfs files cached while open.
* cache.files=full: Enables page caching. Underlying files cached, mergerfs files cached across opens.
* cache.files=auto-full: Enables page caching. Underlying files cached, mergerfs files cached across opens if mtime and size are unchanged since previous open.
* cache.files=libfuse: follow traditional libfuse `direct_io`, `kernel_cache`, and `auto_cache` arguments.


FUSE, which mergerfs uses, offers a number of page caching modes. mergerfs tries to simplify their use via the `cache.files` option. It can and should replace usage of `direct_io`, `kernel_cache`, and `auto_cache`.

Due to mergerfs using FUSE and therefore being a userland process proxying existing filesystems the kernel will double cache the content being read and written through mergerfs. Once from the underlying filesystem and once from mergerfs (it sees them as two separate entities). Using `cache.files=off` will keep the double caching from happening by disabling caching of mergerfs but this has the side effect that *all* read and write calls will be passed to mergerfs which may be slower than enabling caching, you lose shared `mmap` support which can affect apps such as rtorrent, and no read-ahead will take place. The kernel will still cache the underlying filesystem data but that only helps so much given mergerfs will still process all requests.

If you do enable file page caching, `cache.files=partial|full|auto-full`, you should also enable `dropcacheonclose` which will cause mergerfs to instruct the kernel to flush the underlying file's page cache when the file is closed. This behavior is the same as the rsync fadvise / drop cache patch and Feh's nocache project.

If most files are read once through and closed (like media) it is best to enable `dropcacheonclose` regardless of caching mode in order to minimize buffer bloat.

It is difficult to balance memory usage, cache bloat & duplication, and performance. Ideally mergerfs would be able to disable caching for the files it reads/writes but allow page caching for itself. That would limit the FUSE overhead. However, there isn't a good way to achieve this. It would need to open all files with O_DIRECT which places limitations on the what underlying filesystems would be supported and complicates the code.

kernel documentation: https://www.kernel.org/doc/Documentation/filesystems/fuse-io.txt


#### entry & attribute caching

Given the relatively high cost of FUSE due to the kernel <-> userspace round trips there are kernel side caches for file entries and attributes. The entry cache limits the `lookup` calls to mergerfs which ask if a file exists. The attribute cache limits the need to make `getattr` calls to mergerfs which provide file attributes (mode, size, type, etc.). As with the page cache these should not be used if the underlying filesystems are being manipulated at the same time as it could lead to odd behavior or data corruption. The options for setting these are `cache.entry` and `cache.negative_entry` for the entry cache and `cache.attr` for the attributes cache. `cache.negative_entry` refers to the timeout for negative responses to lookups (non-existent files).


#### writeback caching

When `cache.files` is enabled the default is for it to perform writethrough caching. This behavior won't help improve performance as each write still goes one for one through the filesystem. By enabling the FUSE writeback cache small writes may be aggregated by the kernel and then sent to mergerfs as one larger request. This can greatly improve the throughput for apps which write to files inefficiently. The amount the kernel can aggregate is limited by the size of a FUSE message. Read the `fuse_msg_size` section for more details.

There is a small side effect as a result of enabling writeback caching. Underlying files won't ever be opened with O_APPEND or O_WRONLY. The former because the kernel then manages append mode and the latter because the kernel may request file data from mergerfs to populate the write cache. The O_APPEND change means that if a file is changed outside of mergerfs it could lead to corruption as the kernel won't know the end of the file has changed. That said any time you use caching you should keep from using the same file outside of mergerfs at the same time.

Note that if an application is properly sizing writes then writeback caching will have little or no effect. It will only help with writes of sizes below the FUSE message size (128K on older kernels, 1M on newer).


#### policy caching

Policies are run every time a function (with a policy as mentioned above) is called. These policies can be expensive depending on mergerfs' setup and client usage patterns. Generally we wouldn't want to cache policy results because it may result in stale responses if the underlying drives are used directly.

The `open` policy cache will cache the result of an `open` policy for a particular input for `cache.open` seconds or until the file is unlinked. Each file close (release) will randomly chose to clean up the cache of expired entries.

This cache is really only useful in cases where you have a large number of branches and `open` is called on the same files repeatedly (like **Transmission** which opens and closes a file on every read/write presumably to keep file handle usage low).


#### statfs caching

Of the syscalls used by mergerfs in policies the `statfs` / `statvfs` call is perhaps the most expensive. It's used to find out the available space of a drive and whether it is mounted read-only. Depending on the setup and usage pattern these queries can be relatively costly. When `cache.statfs` is enabled all calls to `statfs` by a policy will be cached for the number of seconds its set to.

Example: If the create policy is `mfs` and the timeout is 60 then for that 60 seconds the same drive will be returned as the target for creates because the available space won't be updated for that time.


#### symlink caching

As of version 4.20 Linux supports symlink caching. Significant performance increases can be had in workloads which use a lot of symlinks. Setting `cache.symlinks=true` will result in requesting symlink caching from the kernel only if supported. As a result its safe to enable it on systems prior to 4.20. That said it is disabled by default for now. You can see if caching is enabled by querying the xattr `user.mergerfs.cache.symlinks` but given it must be requested at startup you can not change it at runtime.


#### readdir caching

As of version 4.20 Linux supports readdir caching. This can have a significant impact on directory traversal. Especially when combined with entry (`cache.entry`) and attribute (`cache.attr`) caching. Setting `cache.readdir=true` will result in requesting readdir caching from the kernel on each `opendir`. If the kernel doesn't support readdir caching setting the option to `true` has no effect. This option is configurable at runtime via xattr `user.mergerfs.cache.readdir`.


#### tiered caching

Some storage technologies support what some call "tiered" caching. The placing of usually smaller, faster storage as a transparent cache to larger, slower storage. NVMe, SSD, Optane in front of traditional HDDs for instance.

MergerFS does not natively support any sort of tiered caching. Most users have no use for such a feature and its inclusion would complicate the code. However, there are a few situations where a cache drive could help with a typical mergerfs setup.

1. Fast network, slow drives, many readers: You've a 10+Gbps network with many readers and your regular drives can't keep up.
2. Fast network, slow drives, small'ish bursty writes: You have a 10+Gbps network and wish to transfer amounts of data less than your cache drive but wish to do so quickly.

With #1 its arguable if you should be using mergerfs at all. RAID would probably be the better solution. If you're going to use mergerfs there are other tactics that may help: spreading the data across drives (see the mergerfs.dup tool) and setting `func.open=rand`, using `symlinkify`, or using dm-cache or a similar technology to add tiered cache to the underlying device.

With #2 one could use dm-cache as well but there is another solution which requires only mergerfs and a cronjob.

1. Create 2 mergerfs pools. One which includes just the slow drives and one which has both the fast drives (SSD,NVME,etc.) and slow drives.
2. The 'cache' pool should have the cache drives listed first.
3. The best `create` policies to use for the 'cache' pool would probably be `ff`, `epff`, `lfs`, or `eplfs`. The latter two under the assumption that the cache drive(s) are far smaller than the backing drives. If using path preserving policies remember that you'll need to manually create the core directories of those paths you wish to be cached. Be sure the permissions are in sync. Use `mergerfs.fsck` to check / correct them. You could also tag the slow drives as `=NC` though that'd mean if the cache drives fill you'd get "out of space" errors.
4. Enable `moveonenospc` and set `minfreespace` appropriately. To make sure there is enough room on the "slow" pool you might want to set `minfreespace` to at least as large as the size of the largest cache drive if not larger. This way in the worst case the whole of the cache drive(s) can be moved to the other drives.
5. Set your programs to use the cache pool.
6. Save one of the below scripts or create you're own.
7. Use `cron` (as root) to schedule the command at whatever frequency is appropriate for your workflow.


##### time based expiring

Move files from cache to backing pool based only on the last time the file was accessed. Replace `-atime` with `-amin` if you want minutes rather than days. May want to use the `fadvise` / `--drop-cache` version of rsync or run rsync with the tool "nocache".

*NOTE:* The arguments to these scripts include the cache **drive**. Not the pool with the cache drive. You could have data loss if the source is the cache pool.


```
#!/bin/bash

if [ $# != 3 ]; then
  echo "usage: $0 <cache-drive> <backing-pool> <days-old>"
  exit 1
fi

CACHE="${1}"
BACKING="${2}"
N=${3}

find "${CACHE}" -type f -atime +${N} -printf '%P\n' | \
  rsync --files-from=- -axqHAXWES --preallocate --remove-source-files "${CACHE}/" "${BACKING}/"
```


##### percentage full expiring

Move the oldest file from the cache to the backing pool. Continue till below percentage threshold.

*NOTE:* The arguments to these scripts include the cache **drive**. Not the pool with the cache drive. You could have data loss if the source is the cache pool.

```
#!/bin/bash

if [ $# != 3 ]; then
  echo "usage: $0 <cache-drive> <backing-pool> <percentage>"
  exit 1
fi

CACHE="${1}"
BACKING="${2}"
PERCENTAGE=${3}

set -o errexit
while [ $(df --output=pcent "${CACHE}" | grep -v Use | cut -d'%' -f1) -gt ${PERCENTAGE} ]
do
    FILE=$(find "${CACHE}" -type f -printf '%A@ %P\n' | \
                  sort | \
                  head -n 1 | \
                  cut -d' ' -f2-)
    test -n "${FILE}"
    rsync -axqHAXWESR --preallocate --remove-source-files "${CACHE}/./${FILE}" "${BACKING}/"
done
```


# PERFORMANCE

mergerfs is at its core just a proxy and therefore its theoretical max performance is that of the underlying devices. However, given it is a FUSE filesystem working from userspace there is an increase in overhead relative to kernel based solutions. That said the performance can match the theoretical max but it depends greatly on the system's configuration. Especially when adding network filesystems into the mix there are many variables which can impact performance. Drive speeds and latency, network speeds and latency, general concurrency, read/write sizes, etc. Unfortunately, given the number of variables it has been difficult to find a single set of settings which provide optimal performance. If you're having performance issues please look over the suggestions below (including the benchmarking section.)

NOTE: be sure to read about these features before changing them to understand what behaviors it may impact

* enable (or disable) `splice_move`, `splice_read`, and `splice_write`
* disable `security_capability` and/or `xattr`
* increase cache timeouts `cache.attr`, `cache.entry`, `cache.negative_entry`
* enable (or disable) page caching (`cache.files`)
* enable `cache.writeback`
* enable `cache.open`
* enable `cache.statfs`
* enable `cache.symlinks`
* enable `cache.readdir`
* change the number of worker threads
* disable `posix_acl`
* disable `async_read`
* test theoretical performance using `nullrw` or mounting a ram disk
* use `symlinkify` if your data is largely static and read-only
* use tiered cache drives
* use LVM and LVM cache to place a SSD in front of your HDDs

If you come across a setting that significantly impacts performance please contact trapexit so he may investigate further.


# BENCHMARKING

Filesystems are complicated. They do many things and many of those are interconnected. Additionally, the OS, drivers, hardware, etc. all can impact performance. Therefore, when benchmarking, it is **necessary** that the test focus as narrowly as possible.

For most throughput is the key benchmark. To test throughput `dd` is useful but **must** be used with the correct settings in order to ensure the filesystem or device is actually being tested. The OS can and will cache data. Without forcing synchronous reads and writes and/or disabling caching the values returned will not be representative of the device's true performance.

When benchmarking through mergerfs ensure you only use 1 branch to remove any possibility of the policies complicating the situation. Benchmark the underlying filesystem first and then mount mergerfs over it and test again. If you're experience speeds below your expectation you will need to narrow down precisely which component is leading to the slowdown. Preferably test the following in the order listed (but not combined).

1. Enable `nullrw` mode with `nullrw=true`. This will effectively make reads and writes no-ops. Removing the underlying device / filesystem from the equation. This will give us the top theoretical speeds.
2. Mount mergerfs over `tmpfs`. `tmpfs` is a RAM disk. Extremely high speed and very low latency. This is a more realistic best case scenario. Example: `mount -t tmpfs -o size=2G tmpfs /tmp/tmpfs`
3. Mount mergerfs over a local drive. NVMe, SSD, HDD, etc. If you have more than one I'd suggest testing each of them as drives and/or controllers (their drivers) could impact performance.
4. Finally, if you intend to use mergerfs with a network filesystem, either as the source of data or to combine with another through mergerfs, test each of those alone as above.

Once you find the component which has the performance issue you can do further testing with different options to see if they impact performance. For reads and writes the most relevant would be: `cache.files`, `async_read`, `splice_move`, `splice_read`, `splice_write`. Less likely but relevant when using NFS or with certain filesystems would be `security_capability`, `xattr`, and `posix_acl`. If you find a specific system, drive, filesystem, controller, etc. that performs poorly contact trapexit so he may investigate further.

Sometimes the problem is really the application accessing or writing data through mergerfs. Some software use small buffer sizes which can lead to more requests and therefore greater overhead. You can test this out yourself by replace `bs=1M` in the examples below with `ibs` or `obs` and using a size of `512` instead of `1M`. In one example test using `nullrw` the write speed dropped from 4.9GB/s to 69.7MB/s when moving from `1M` to `512`. Similar results were had when testing reads. Small writes overhead may be improved by leveraging a write cache but in casual tests little gain was found. More tests will need to be done before this feature would become available. If you have an app that appears slow with mergerfs it could be due to this. Contact trapexit so he may investigate further.


### write benchmark

```
$ dd if=/dev/zero of=/mnt/mergerfs/1GB.file bs=1M count=1024 oflag=nocache conv=fdatasync status=progress
```

### read benchmark

```
$ dd if=/mnt/mergerfs/1GB.file of=/dev/null bs=1M count=1024 iflag=nocache conv=fdatasync status=progress
```


# TIPS / NOTES

* This document is very literal and thorough. Unless there is a bug things work as described. If a suspected feature isn't mentioned it doesn't exist.
* Ensure you're using the latest version. Few distros have the latest version.
* **use_ino** will only work when used with mergerfs 2.18.0 and above.
* Run mergerfs as `root` (with **allow_other**) unless you're merging paths which are owned by the same user otherwise strange permission issues may arise.
* https://github.com/trapexit/backup-and-recovery-howtos : A set of guides / howtos on creating a data storage system, backing it up, maintaining it, and recovering from failure.
* If you don't see some directories and files you expect in a merged point or policies seem to skip drives be sure the user has permission to all the underlying directories. Use `mergerfs.fsck` to audit the drive for out of sync permissions.
* Do **not** use `cache.files=off` if you expect applications (such as rtorrent) to use [mmap](http://linux.die.net/man/2/mmap) files. Shared mmap is not currently supported in FUSE w/ page caching disabled. Enabling `dropcacheonclose` is recommended when `cache.files=partial|full|auto-full`.
* [Kodi](http://kodi.tv), [Plex](http://plex.tv), [Subsonic](http://subsonic.org), etc. can use directory [mtime](http://linux.die.net/man/2/stat) to more efficiently determine whether to scan for new content rather than simply performing a full scan. If using the default **getattr** policy of **ff** it's possible those programs will miss an update on account of it returning the first directory found's **stat** info and its a later directory on another mount which had the **mtime** recently updated. To fix this you will want to set **func.getattr=newest**. Remember though that this is just **stat**. If the file is later **open**'ed or **unlink**'ed and the policy is different for those then a completely different file or directory could be acted on.
* Some policies mixed with some functions may result in strange behaviors. Not that some of these behaviors and race conditions couldn't happen outside **mergerfs** but that they are far more likely to occur on account of the attempt to merge together multiple sources of data which could be out of sync due to the different policies.
* For consistency its generally best to set **category** wide policies rather than individual **func**'s. This will help limit the confusion of tools such as [rsync](http://linux.die.net/man/1/rsync). However, the flexibility is there if needed.


# KNOWN ISSUES / BUGS

#### kernel issues & bugs

[https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs](https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs)


#### directory mtime is not being updated

Remember that the default policy for `getattr` is `ff`. The information for the first directory found will be returned. If it wasn't the directory which had been updated then it will appear outdated.

The reason this is the default is because any other policy would be more expensive and for many applications it is unnecessary. To always return the directory with the most recent mtime or a faked value based on all found would require a scan of all drives.

If you always want the directory information from the one with the most recent mtime then use the `newest` policy for `getattr`.


#### 'mv /mnt/pool/foo /mnt/disk1/foo' removes 'foo'

This is not a bug.

Run in verbose mode to better understand what's happening:

```
$ mv -v /mnt/pool/foo /mnt/disk1/foo
copied '/mnt/pool/foo' -> '/mnt/disk1/foo'
removed '/mnt/pool/foo'
$ ls /mnt/pool/foo
ls: cannot access '/mnt/pool/foo': No such file or directory
```

`mv`, when working across devices, is copying the source to target and then removing the source. Since the source **is** the target in this case, depending on the unlink policy, it will remove the just copied file and other files across the branches.

If you want to move files to one drive just copy them there and use mergerfs.dedup to clean up the old paths or manually remove them from the branches directly.


#### cached memory appears greater than it should be

Use `cache.files=off` and/or `dropcacheonclose=true`. See the section on page caching.


#### NFS clients returning ESTALE / Stale file handle

NFS does not like out of band changes. That is especially true of inode values.

Be sure to use the following options:

* noforget
* use_ino
* inodecalc=path-hash


#### rtorrent fails with ENODEV (No such device)

Be sure to set `cache.files=partial|full|auto-full` or turn off `direct_io`. rtorrent and some other applications use [mmap](http://linux.die.net/man/2/mmap) to read and write to files and offer no fallback to traditional methods. FUSE does not currently support mmap while using `direct_io`. There may be a performance penalty on writes with `direct_io` off as well as the problem of double caching but it's the only way to get such applications to work. If the performance loss is too high for other apps you can mount mergerfs twice. Once with `direct_io` enabled and one without it. Be sure to set `dropcacheonclose=true` if not using `direct_io`.


#### Plex doesn't work with mergerfs

It does. If you're trying to put Plex's config / metadata / database on mergerfs you can't set `cache.files=off` because Plex is using sqlite3 with mmap enabled. Shared mmap is not supported by Linux's FUSE implementation when page caching is disabled. To fix this place the data elsewhere (preferable) or enable `cache.files` (with `dropcacheonclose=true`). Sqlite3 does not need mmap but the developer needs to fall back to standard IO if mmap fails.

If the issue is that scanning doesn't seem to pick up media then be sure to set `func.getattr=newest` though generally a full scan will pick up all media anyway.


#### When a program tries to move or rename a file it fails

Please read the section above regarding [rename & link](#rename--link).

The problem is that many applications do not properly handle `EXDEV` errors which `rename` and `link` may return even though they are perfectly valid situations which do not indicate actual drive or OS errors. The error will only be returned by mergerfs if using a path preserving policy as described in the policy section above. If you do not care about path preservation simply change the mergerfs policy to the non-path preserving version. For example: `-o category.create=mfs`

Ideally the offending software would be fixed and it is recommended that if you run into this problem you contact the software's author and request proper handling of `EXDEV` errors.


#### my 32bit software has problems

Some software have problems with 64bit inode values. The symptoms can include EOVERFLOW errors when trying to list files. You can address this by setting `inodecalc` to one of the 32bit based algos as described in the relevant section.


#### Samba: Moving files / directories fails

Workaround: Copy the file/directory and then remove the original rather than move.

This isn't an issue with Samba but some SMB clients. GVFS-fuse v1.20.3 and prior (found in Ubuntu 14.04 among others) failed to handle certain error codes correctly. Particularly **STATUS_NOT_SAME_DEVICE** which comes from the **EXDEV** which is returned by **rename** when the call is crossing mount points. When a program gets an **EXDEV** it needs to explicitly take an alternate action to accomplish its goal. In the case of **mv** or similar it tries **rename** and on **EXDEV** falls back to a manual copying of data between the two locations and unlinking the source. In these older versions of GVFS-fuse if it received **EXDEV** it would translate that into **EIO**. This would cause **mv** or most any application attempting to move files around on that SMB share to fail with a IO error.

[GVFS-fuse v1.22.0](https://bugzilla.gnome.org/show_bug.cgi?id=734568) and above fixed this issue but a large number of systems use the older release. On Ubuntu the version can be checked by issuing `apt-cache showpkg gvfs-fuse`. Most distros released in 2015 seem to have the updated release and will work fine but older systems may not. Upgrading gvfs-fuse or the distro in general will address the problem.

In Apple's MacOSX 10.9 they replaced Samba (client and server) with their own product. It appears their new client does not handle **EXDEV** either and responds similar to older release of gvfs on Linux.


#### Trashing files occasionally fails

This is the same issue as with Samba. `rename` returns `EXDEV` (in our case that will really only happen with path preserving policies like `epmfs`) and the software doesn't handle the situation well. This is unfortunately a common failure of software which moves files around. The standard indicates that an implementation `MAY` choose to support non-user home directory trashing of files (which is a `MUST`). The implementation `MAY` also support "top directory trashes" which many probably do.

To create a `$topdir/.Trash` directory as defined in the standard use the [mergerfs-tools](https://github.com/trapexit/mergerfs-tools) tool `mergerfs.mktrash`.

#### tar: Directory renamed before its status could be extracted

Make sure to use the `use_ino` option.


#### Supplemental user groups

Due to the overhead of [getgroups/setgroups](http://linux.die.net/man/2/setgroups) mergerfs utilizes a cache. This cache is opportunistic and per thread. Each thread will query the supplemental groups for a user when that particular thread needs to change credentials and will keep that data for the lifetime of the thread. This means that if a user is added to a group it may not be picked up without the restart of mergerfs. However, since the high level FUSE API's (at least the standard version) thread pool dynamically grows and shrinks it's possible that over time a thread will be killed and later a new thread with no cache will start and query the new data.

The gid cache uses fixed storage to simplify the design and be compatible with older systems which may not have C++11 compilers. There is enough storage for 256 users' supplemental groups. Each user is allowed up to 32 supplemental groups. Linux >= 2.6.3 allows up to 65535 groups per user but most other *nixs allow far less. NFS allowing only 16. The system does handle overflow gracefully. If the user has more than 32 supplemental groups only the first 32 will be used. If more than 256 users are using the system when an uncached user is found it will evict an existing user's cache at random. So long as there aren't more than 256 active users this should be fine. If either value is too low for your needs you will have to modify `gidcache.hpp` to increase the values. Note that doing so will increase the memory needed by each thread.

While not a bug some users have found when using containers that supplemental groups defined inside the container don't work properly with regard to permissions. This is expected as mergerfs lives outside the container and therefore is querying the host's group database. There might be a hack to work around this (make mergerfs read the /etc/group file in the container) but it is not yet implemented and would be limited to Linux and the /etc/group DB. Preferably users would mount in the host group file into the containers or use a standard shared user & groups technology like NIS or LDAP.


#### mergerfs or libfuse crashing

First... always upgrade to the latest version unless told otherwise.

If using mergerfs below 2.22.0:

If suddenly the mergerfs mount point disappears and `Transport endpoint is not connected` is returned when attempting to perform actions within the mount directory **and** the version of libfuse (use `mergerfs -v` to find the version) is older than `2.9.4` its likely due to a bug in libfuse. Affected versions of libfuse can be found in Debian Wheezy, Ubuntu Precise and others.

In order to fix this please install newer versions of libfuse. If using a Debian based distro (Debian,Ubuntu,Mint) you can likely just install newer versions of [libfuse](https://packages.debian.org/unstable/libfuse2) and [fuse](https://packages.debian.org/unstable/fuse) from the repo of a newer release.

If using mergerfs at or above 2.22.0:

First upgrade if possible, check the known bugs section, and contact trapexit.


#### mergerfs appears to be crashing or exiting

There seems to be an issue with Linux version `4.9.0` and above in which an invalid message appears to be transmitted to libfuse (used by mergerfs) causing it to exit. No messages will be printed in any logs as it's not a proper crash. Debugging of the issue is still ongoing and can be followed via the [fuse-devel thread](https://sourceforge.net/p/fuse/mailman/message/35662577).


#### rm: fts_read failed: No such file or directory

Please update. This is only happened to mergerfs versions at or below v2.25.x and will not occur in more recent versions.


# FAQ

#### How well does mergerfs scale? Is it "production ready?"

Users have reported running mergerfs on everything from a Raspberry Pi to dual socket Xeon systems with >20 cores. I'm aware of at least a few companies which use mergerfs in production. [Open Media Vault](https://www.openmediavault.org) includes mergerfs as its sole solution for pooling drives. The author of mergerfs had it running for over 300 days managing 16+ drives with reasonably heavy 24/7 read and write usage. Stopping only after the machine's power supply died.

Most serious issues (crashes or data corruption) have been due to [kernel bugs](https://github.com/trapexit/mergerfs/wiki/Kernel-Issues-&-Bugs). All of which are fixed in stable releases.


#### Can mergerfs be used with drives which already have data / are in use?

Yes. MergerFS is a proxy and does **NOT** interfere with the normal form or function of the drives / mounts / paths it manages.

MergerFS is **not** a traditional filesystem. MergerFS is **not** RAID. It does **not** manipulate the data that passes through it. It does **not** shard data across drives. It merely shards some **behavior** and aggregates others.


#### Can mergerfs be removed without affecting the data?

See the previous question's answer.


#### What policies should I use?

Unless you're doing something more niche the average user is probably best off using `mfs` for `category.create`. It will spread files out across your branches based on available space. Use `mspmfs` if you want to try to colocate the data a bit more. You may want to use `lus` if you prefer a slightly different distribution of data if you have a mix of smaller and larger drives. Generally though `mfs`, `lus`, or even `rand` are good for the general use case. If you are starting with an imbalanced pool you can use the tool **mergerfs.balance** to redistribute files across the pool.

If you really wish to try to colocate files based on directory you can set `func.create` to `epmfs` or similar and `func.mkdir` to `rand` or `eprand` depending on if you just want to colocate generally or on specific branches. Either way the *need* to colocate is rare. For instance: if you wish to remove the drive regularly and want the data to predictably be on that drive or if you don't use backup at all and don't wish to replace that data piecemeal. In which case using path preservation can help but will require some manual attention. Colocating after the fact can be accomplished using the **mergerfs.consolidate** tool. If you don't need strict colocation which the `ep` policies provide then you can use the `msp` based policies which will walk back the path till finding a branch that works.

Ultimately there is no correct answer. It is a preference or based on some particular need. mergerfs is very easy to test and experiment with. I suggest creating a test setup and experimenting to get a sense of what you want.

The reason `mfs` is not the default `category.create` policy is historical. When/if a 3.X gets released it will be changed to minimize confusion people often have with path preserving policies.


#### What settings should I use?

Depends on what features you want. Generally speaking there are no "wrong" settings. All settings are performance or feature related. The best bet is to read over the available options and choose what fits your situation. If something isn't clear from the documentation please reach out and the documentation will be improved.

That said, for the average person, the following should be fine:

`-o use_ino,cache.files=off,dropcacheonclose=true,allow_other,category.create=mfs`


#### Why are all my files ending up on 1 drive?!

Did you start with empty drives? Did you explicitly configure a `category.create` policy? Are you using an `existing path` / `path preserving` policy?

The default create policy is `epmfs`. That is a path preserving algorithm. With such a policy for `mkdir` and `create` with a set of empty drives it will select only 1 drive when the first directory is created. Anything, files or directories, created in that first directory will be placed on the same branch because it is preserving paths.

This catches a lot of new users off guard but changing the default would break the setup for many existing users. If you do not care about path preservation and wish your files to be spread across all your drives change to `mfs` or similar policy as described above. If you do want path preservation you'll need to perform the manual act of creating paths on the drives you want the data to land on before transferring your data. Setting `func.mkdir=epall` can simplify managing path preservation for `create`. Or use `func.mkdir=rand` if you're interested in just grouping together directory content by drive.


#### Do hardlinks work?

Yes. You need to use `use_ino` to support proper reporting of inodes but they work regardless. See also the option `inodecalc`.

What mergerfs does not do is fake hard links across branches.  Read the section "rename & link" for how it works.

Remember that hardlinks will NOT work across devices. That includes between the original filesystem and a mergerfs pool, between two separate pools of the same underlying filesystems, or bind mounts of paths within the mergerfs pool. The latter is common when using Docker or Podman. Multiple volumes (bind mounts) to the same underlying filesystem are considered different devices. There is no way to link between them. You should mount in the highest directory in the mergerfs pool that includes all the paths you need if you want links to work.


#### Can I use mergerfs without SnapRAID? SnapRAID without mergerfs?

Yes. They are completely unreleated pieces of software.


#### Does mergerfs support CoW / copy-on-write / writes to read-only filesystems?

Not in the sense of a filesystem like BTRFS or ZFS nor in the overlayfs or aufs sense. It does offer a [cow-shell](http://manpages.ubuntu.com/manpages/bionic/man1/cow-shell.1.html) like hard link breaking (copy to temp file then rename over original) which can be useful when wanting to save space by hardlinking duplicate files but wish to treat each name as if it were a unique and separate file.

If you want to write to a read-only filesystem you should look at overlayfs. You can always include the overlayfs mount into a mergerfs pool.


#### Why can't I see my files / directories?

It's almost always a permissions issue. Unlike mhddfs and unionfs-fuse, which runs as root and attempts to access content as such, mergerfs always changes its credentials to that of the caller. This means that if the user does not have access to a file or directory than neither will mergerfs. However, because mergerfs is creating a union of paths it may be able to read some files and directories on one drive but not another resulting in an incomplete set.

Whenever you run into a split permission issue (seeing some but not all files) try using [mergerfs.fsck](https://github.com/trapexit/mergerfs-tools) tool to check for and fix the mismatch. If you aren't seeing anything at all be sure that the basic permissions are correct. The user and group values are correct and that directories have their executable bit set. A common mistake by users new to Linux is to `chmod -R 644` when they should have `chmod -R u=rwX,go=rX`.

If using a network filesystem such as NFS, SMB, CIFS (Samba) be sure to pay close attention to anything regarding permissioning and users. Root squashing and user translation for instance has bitten a few mergerfs users. Some of these also affect the use of mergerfs from container platforms such as Docker.


#### Is my OS's libfuse needed for mergerfs to work?

No. Normally `mount.fuse` is needed to get mergerfs (or any FUSE filesystem to mount using the `mount` command but in vendoring the libfuse library the `mount.fuse` app has been renamed to `mount.mergerfs` meaning the filesystem type in `fstab` can simply be `mergerfs`. That said there should be no harm in having it installed and continuing to using `fuse.mergerfs` as the type in `/etc/fstab`.

If `mergerfs` doesn't work as a type it could be due to how the `mount.mergerfs` tool was installed. Must be in `/sbin/` with proper permissions.


#### Why was libfuse embedded into mergerfs?

1. A significant number of users use mergerfs on distros with old versions of libfuse which have serious bugs. Requiring updated versions of libfuse on those distros isn't practical (no package offered, user inexperience, etc.). The only practical way to provide a stable runtime on those systems was to "vendor" / embed the library into the project.
2. mergerfs was written to use the high level API. There are a number of limitations in the HLAPI that make certain features difficult or impossible to implement. While some of these features could be patched into newer versions of libfuse without breaking the public API some of them would require hacky code to provide backwards compatibility. While it may still be worth working with upstream to address these issues in future versions, since the library needs to be vendored for stability and compatibility reasons it is preferable / easier to modify the API. Longer term the plan is to rewrite mergerfs to use the low level API.


#### Why did support for system libfuse get removed?

See above first.

If/when mergerfs is rewritten to use the low-level API then it'll be plausible to support system libfuse but till then it's simply too much work to manage the differences across the versions.


#### Why use mergerfs over mhddfs?

mhddfs is no longer maintained and has some known stability and security issues (see below). MergerFS provides a superset of mhddfs' features and should offer the same or maybe better performance.

Below is an example of mhddfs and mergerfs setup to work similarly.

`mhddfs -o mlimit=4G,allow_other /mnt/drive1,/mnt/drive2 /mnt/pool`

`mergerfs -o minfreespace=4G,allow_other,category.create=ff /mnt/drive1:/mnt/drive2 /mnt/pool`


#### Why use mergerfs over aufs?

aufs is mostly abandoned and no longer available in many distros.

While aufs can offer better peak performance mergerfs provides more configurability and is generally easier to use. mergerfs however does not offer the overlay / copy-on-write (CoW) features which aufs and overlayfs have.


#### Why use mergerfs over unionfs?

UnionFS is more like aufs than mergerfs in that it offers overlay / CoW features. If you're just looking to create a union of drives and want flexibility in file/directory placement then mergerfs offers that whereas unionfs is more for overlaying RW filesystems over RO ones.


#### Why use mergerfs over overlayfs?

Same reasons as with unionfs.


#### Why use mergerfs over LVM/ZFS/BTRFS/RAID0 drive concatenation / striping?

With simple JBOD / drive concatenation / stripping / RAID0 a single drive failure will result in full pool failure. mergerfs performs a similar function without the possibility of catastrophic failure and the difficulties in recovery. Drives may fail, however, all other data will continue to be accessible.

When combined with something like [SnapRaid](http://www.snapraid.it) and/or an offsite backup solution you can have the flexibility of JBOD without the single point of failure.


#### Why use mergerfs over ZFS?

MergerFS is not intended to be a replacement for ZFS. MergerFS is intended to provide flexible pooling of arbitrary drives (local or remote), of arbitrary sizes, and arbitrary filesystems. For `write once, read many` usecases such as bulk media storage. Where data integrity and backup is managed in other ways. In that situation ZFS can introduce a number of costs and limitations as described [here](http://louwrentius.com/the-hidden-cost-of-using-zfs-for-your-home-nas.html), [here](https://markmcb.com/2020/01/07/five-years-of-btrfs/), and [here](https://utcc.utoronto.ca/~cks/space/blog/solaris/ZFSWhyNoRealReshaping).


#### Why use mergerfs over UnRAID?

UnRAID is a full OS and its storage layer, as I understand, is proprietary and closed source. Users who have experience with both have said they prefer the flexibility offered by mergerfs and for some the fact it is free and open source is important.

There are a number of UnRAID users who use mergerfs as well though I'm not entirely familiar with the use case.


#### What should mergerfs NOT be used for?

* databases: Even if the database stored data in separate files (mergerfs wouldn't offer much otherwise) the higher latency of the indirection will kill performance. If it is a lightly used SQLITE database then it may be fine but you'll need to test.

* VM images: For the same reasons as databases. VM images are accessed very aggressively and mergerfs will introduce too much latency (if it works at all).

* As replacement for RAID: mergerfs is just for pooling branches. If you need that kind of device performance aggregation or high availability you should stick with RAID.


#### Can drives be written to directly? Outside of mergerfs while pooled?

Yes, however it's not recommended to use the same file from within the pool and from without at the same time (particularly writing). Especially if using caching of any kind (cache.files, cache.entry, cache.attr, cache.negative_entry, cache.symlinks, cache.readdir, etc.) as there could be a conflict between cached data and not.


#### Why do I get an "out of space" / "no space left on device" / ENOSPC error even though there appears to be lots of space available?

First make sure you've read the sections above about policies, path preservation, branch filtering, and the options **minfreespace**, **moveonenospc**, **statfs**, and **statfs_ignore**.

mergerfs is simply presenting a union of the content within multiple branches. The reported free space is an aggregate of space available within the pool (behavior modified by **statfs** and **statfs_ignore**). It does not represent a contiguous space. In the same way that read-only filesystems, those with quotas, or reserved space report the full theoretical space available.

Due to path preservation, branch tagging, read-only status, and **minfreespace** settings it is perfectly valid that `ENOSPC` / "out of space" / "no space left on device" be returned. It is doing what was asked of it: filtering possible branches due to those settings. Only one error can be returned and if one of the reasons for filtering a branch was **minfreespace** then it will be returned as such. **moveonenospc** is only relevant to writing a file which is too large for the drive its currently on.

It is also possible that the filesystem selected has run out of inodes. Use `df -i` to list the total and available inodes per filesystem.

If you don't care about path preservation then simply change the `create` policy to one which isn't. `mfs` is probably what most are looking for. The reason it's not default is because it was originally set to `epmfs` and changing it now would change people's setup. Such a setting change will likely occur in mergerfs 3.


#### Why does the total available space in mergerfs not equal outside?

Are you using ext2/3/4? With reserve for root? mergerfs uses available space for statfs calculations. If you've reserved space for root then it won't show up.

You can remove the reserve by running: `tune2fs -m 0 <device>`


#### Can mergerfs mounts be exported over NFS?

Yes, however if you do anything which may changes files out of band (including for example using the `newest` policy) it will result in "stale file handle" errors unless properly setup.

Be sure to use the following options:

* noforget
* use_ino
* inodecalc=path-hash


#### Can mergerfs mounts be exported over Samba / SMB?

Yes. While some users have reported problems it appears to always be related to how Samba is setup in relation to permissions.


#### Can mergerfs mounts be used over SSHFS?

Yes.


#### I notice massive slowdowns of writes when enabling cache.files.

When file caching is enabled in any form (`cache.files!=off` or `direct_io=false`) it will issue `getxattr` requests for `security.capability` prior to *every single write*. This will usually result in a performance degradation, especially when using a network filesystem (such as NFS or CIFS/SMB/Samba.) Unfortunately at this moment the kernel is not caching the response.

To work around this situation mergerfs offers a few solutions.

1. Set `security_capability=false`. It will short circuit any call and return `ENOATTR`. This still means though that mergerfs will receive the request before every write but at least it doesn't get passed through to the underlying filesystem.
2. Set `xattr=noattr`. Same as above but applies to *all* calls to getxattr. Not just `security.capability`. This will not be cached by the kernel either but mergerfs' runtime config system will still function.
3. Set `xattr=nosys`. Results in mergerfs returning `ENOSYS` which *will* be cached by the kernel. No future xattr calls will be forwarded to mergerfs. The downside is that also means the xattr based config and query functionality won't work either.
4. Disable file caching. If you aren't using applications which use `mmap` it's probably simpler to just disable it all together. The kernel won't send the requests when caching is disabled.


#### What are these .fuse_hidden files?

Please upgrade. mergerfs >= 2.26.0 will not have these temporary files. See the notes on `unlink`.


#### It's mentioned that there are some security issues with mhddfs. What are they? How does mergerfs address them?

[mhddfs](https://github.com/trapexit/mhddfs) manages running as **root** by calling [getuid()](https://github.com/trapexit/mhddfs/blob/cae96e6251dd91e2bdc24800b4a18a74044f6672/src/main.c#L319) and if it returns **0** then it will [chown](http://linux.die.net/man/1/chown) the file. Not only is that a race condition but it doesn't handle other situations. Rather than attempting to simulate POSIX ACL behavior the proper way to manage this is to use [seteuid](http://linux.die.net/man/2/seteuid) and [setegid](http://linux.die.net/man/2/setegid), in effect becoming the user making the original call, and perform the action as them. This is what mergerfs does and why mergerfs should always run as root.

In Linux setreuid syscalls apply only to the thread. GLIBC hides this away by using realtime signals to inform all threads to change credentials. Taking after **Samba**, mergerfs uses **syscall(SYS_setreuid,...)** to set the callers credentials for that thread only. Jumping back to **root** as necessary should escalated privileges be needed (for instance: to clone paths between drives).

For non-Linux systems mergerfs uses a read-write lock and changes credentials only when necessary. If multiple threads are to be user X then only the first one will need to change the processes credentials. So long as the other threads need to be user X they will take a readlock allowing multiple threads to share the credentials. Once a request comes in to run as user Y that thread will attempt a write lock and change to Y's credentials when it can. If the ability to give writers priority is supported then that flag will be used so threads trying to change credentials don't starve. This isn't the best solution but should work reasonably well assuming there are few users.


# SUPPORT

Filesystems are complex and difficult to debug. mergerfs, while being just a proxy of sorts, is also very difficult to debug given the large number of possible settings it can have itself and the massive number of environments it can run in. When reporting on a suspected issue **please, please** include as much of the below information as possible otherwise it will be difficult or impossible to diagnose. Also please make sure to read all of the above documentation as it includes nearly every known system or user issue previously encountered.

**Please make sure you are using the [latest release](https://github.com/trapexit/mergerfs/releases) or have tried it in comparison. Old versions, which are often included in distros like Debian and Ubuntu, are not ever going to be updated and your bug may have been addressed already.**


#### Information to include in bug reports

* Version of mergerfs: `mergerfs -V`
* mergerfs settings: from `/etc/fstab` or command line execution
* Version of Linux: `uname -a`
* Versions of any additional software being used
* List of drives, their filesystems, and sizes (before and after issue): `df -h`
* **All** information about the relevant branches and paths: permissions, etc.
* A `strace` of the app having problems:
  * `strace -fvTtt -s 256 -o /tmp/app.strace.txt <cmd>`
* A `strace` of mergerfs while the program is trying to do whatever it's failing to do:
  * `strace -fvTtt -s 256 -p <mergerfsPID> -o /tmp/mergerfs.strace.txt`
* **Precise** directions on replicating the issue. Do not leave **anything** out.
* Try to recreate the problem in the simplest way using standard programs.


#### Contact / Issue submission

* github.com: https://github.com/trapexit/mergerfs/issues
* email: trapexit@spawn.link
* twitter: https://twitter.com/_trapexit
* reddit: https://www.reddit.com/user/trapexit
* discord: https://discord.gg/MpAr69V


#### Support development

This software is free to use and released under a very liberal license (ISC). That said if you like this software and would like to support its development donations are welcome.

At the moment my preference would be GitHub Sponsors only because I am part of the matching program. That said please use whatever platform you prefer.

* GitHub Sponsors: https://github.com/sponsors/trapexit
* PayPal: https://paypal.me/trapexit
* Patreon: https://www.patreon.com/trapexit
* Ko-Fi: https://ko-fi.com/trapexit
* Open Collective: https://opencollective.com/trapexit
* Bitcoin (BTC): bc1qu537hqlnmn2wawx9n7nws0dlkz55h0cd93ny28
* Bitcoin Cash (BCH): bitcoincash:qqp0vh9v44us74gaggwjfv9y54zfjmmd7srlqxa3xt
* Bitcoin SV (BSV): 1FkFuxRtt3f8LbkpeUKRZq7gKJFzGSGgZV
* Bitcoin Gold (BTG): AaPuJgJeohPjkB3LxJM6NKGnaHoRJ8ieT3
* Litecoin (LTC): MJQzsHBdNnkyGqCFdcAdHYKugicBmfAXfQ
* Dogecoin (DOGE): DLJNLVe28vZ4SMQSxDJLBQBv57rGtUoWFh
* Ethereum (ETH): 0xB8d6d55c0319aacC327860d13f891427caEede7a
  * Basic Attention Token (BAT): 0xB8d6d55c0319aacC327860d13f891427caEede7a
  * Chainlink (LINK): 0xB8d6d55c0319aacC327860d13f891427caEede7a
  * Reserve Rights (RSR): 0xB8d6d55c0319aacC327860d13f891427caEede7a
  * Reef Finance (REEF): 0xB8d6d55c0319aacC327860d13f891427caEede7a
  * Any ERC20 Token: 0xB8d6d55c0319aacC327860d13f891427caEede7a
* Ethereum Classic (ETC): 0x2B6054428e69a1201B6555f7a2aEc0Fba01EAD9F
* Dash (DASH): XvsFrohu8tbjA4E8p7xsc86E2ADxLHGXHL
* Monero (XMR): 45BBZMrJwPSaFwSoqLVNEggWR2BJJsXxz7bNz8FXnnFo3GyhVJFSCrCFSS7zYwDa9r1TmFmGMxQ2HTntuc11yZ9q1LeCE8f
* Filecoin (FIL): f1wpypkjcluufzo74yha7p67nbxepzizlroockgcy
* LBRY Credits (LBC): bFusyoZPkSuzM2Pr8mcthgvkymaosJZt5r
* Ripple (XRP): r9f6aoxaGD8aymxqH89Ke1PCUPkNiFdZZC
* Tezos (XTZ): tz1ZxerkbbALsuU9XGV9K9fFpuLWnKAGfc1C
* Zcash (ZEC): t1Zo1GGn2T3GrhKvgdtnTsTnWu6tCPaCaHG
* DigiByte (DGB): Sb8r1qTrryY9Sp4YkTE1eeKEGVzgArnE5N
* Namecoin (NMC): NDzb9FkoptGu5QbgetCkodJqo2zE1cTwyb
* Vertcoin (VTC): 3PYdhokAGXJwWrwHRoTywxG4iUDk6EHjKe
* Other crypto currencies: contact me for address


# LINKS

* https://spawn.link
* https://github.com/trapexit/mergerfs
* https://github.com/trapexit/mergerfs/wiki
* https://github.com/trapexit/mergerfs-tools
* https://github.com/trapexit/scorch
* https://github.com/trapexit/bbf
* https://github.com/trapexit/backup-and-recovery-howtos

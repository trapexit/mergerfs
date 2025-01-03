# Options

These options are the same regardless of whether you use them with the
`mergerfs` commandline program, in fstab, or in a config file.

### types

- BOOL = 'true' | 'false'
- INT = [MIN_INT,MAX_INT]
- UINT = [0,MAX_INT]
- SIZE = 'NNM'; NN = INT, M = 'K' | 'M' | 'G' | 'T'
- STR = string (may refer to an enumerated value, see details of
  argument)
- FUNC = filesystem function
- CATEGORY = function category
- POLICY = mergerfs function policy


### mount options

- **config**: Path to a config file. Same arguments as below in
  key=val / ini style format.
- **branches**: Colon delimited list of branches.
- **minfreespace=SIZE**: The minimum space value used for creation
  policies. Can be overridden by branch specific option. Understands
  'K', 'M', and 'G' to represent kilobyte, megabyte, and gigabyte
  respectively. (default: 4G)
- **moveonenospc=BOOL|POLICY**: When enabled if a **write** fails with
  **ENOSPC** (no space left on device) or **EDQUOT** (disk quota
  exceeded) the policy selected will run to find a new location for
  the file. An attempt to move the file to that branch will occur
  (keeping all metadata possible) and if successful the original is
  unlinked and the write retried. (default: false, true = mfs)
- **inodecalc=passthrough|path-hash|devino-hash|hybrid-hash**: Selects
  the inode calculation algorithm. (default: hybrid-hash)
- **dropcacheonclose=BOOL**: When a file is requested to be closed
  call `posix_fadvise` on it first to instruct the kernel that we no
  longer need the data and it can drop its cache. Recommended when
  **cache.files=partial|full|auto-full|per-process** to limit double
  caching. (default: false)
- **direct-io-allow-mmap=BOOL**: On newer kernels (>= 6.6) it is
  possible to disable file page caching while still allowing for
  shared mmap support. mergerfs will enable this feature if available
  but an option is provided to turn it off for testing and debugging
  purposes. (default: true)
- **symlinkify=BOOL**: When enabled and a file is not writable and its
  mtime or ctime is older than **symlinkify_timeout** files will be
  reported as symlinks to the original files. Please read more below
  before using. (default: false)
- **symlinkify_timeout=UINT**: Time to wait, in seconds, to activate
  the **symlinkify** behavior. (default: 3600)
- **nullrw=BOOL**: Turns reads and writes into no-ops. The request
  will succeed but do nothing. Useful for benchmarking
  mergerfs. (default: false)
- **lazy-umount-mountpoint=BOOL**: mergerfs will attempt to "lazy
  umount" the mountpoint before mounting itself. Useful when
  performing live upgrades of mergerfs. (default: false)
- **ignorepponrename=BOOL**: Ignore path preserving on
  rename. Typically rename and link act differently depending on the
  policy of `create` (read below). Enabling this will cause rename and
  link to always use the non-path preserving behavior. This means
  files, when renamed or linked, will stay on the same
  filesystem. (default: false)
- **export-support=BOOL**: Sets a low-level FUSE feature intended to
  indicate the filesystem can support being exported via
  NFS. (default: true)
- **security_capability=BOOL**: If false return ENOATTR when xattr
  security.capability is queried. (default: true)
- **xattr=passthrough|noattr|nosys**: Runtime control of
  xattrs. Default is to passthrough xattr requests. 'noattr' will
  short circuit as if nothing exists. 'nosys' will respond with ENOSYS
  as if xattrs are not supported or disabled. (default: passthrough)
- **link_cow=BOOL**: When enabled if a regular file is opened which
  has a link count > 1 it will copy the file to a temporary file and
  rename over the original. Breaking the link and providing a basic
  copy-on-write function similar to cow-shell. (default: false)
- **statfs=base|full**: Controls how statfs works. 'base' means it
  will always use all branches in statfs calculations. 'full' is in
  effect path preserving and only includes branches where the path
  exists. (default: base)
- **statfs_ignore=none|ro|nc**: 'ro' will cause statfs calculations to
  ignore available space for branches mounted or tagged as 'read-only'
  or 'no create'. 'nc' will ignore available space for branches tagged
  as 'no create'. (default: none)
- **nfsopenhack=off|git|all**: A workaround for exporting mergerfs
  over NFS where there are issues with creating files for write while
  setting the mode to read-only. (default: off)
- **branches-mount-timeout=UINT**: Number of seconds to wait at
  startup for branches to be a mount other than the mountpoint's
  filesystem. (default: 0)
- **follow-symlinks=never|directory|regular|all**: Turns symlinks into
  what they point to. (default: never)
- **link-exdev=passthrough|rel-symlink|abs-base-symlink|abs-pool-symlink**:
  When a link fails with EXDEV optionally create a symlink to the file
  instead.
- **rename-exdev=passthrough|rel-symlink|abs-symlink**: When a rename
  fails with EXDEV optionally move the file to a special directory and
  symlink to it.
- **readahead=UINT**: Set readahead (in kilobytes) for mergerfs and
  branches if greater than 0. (default: 0)
- **posix_acl=BOOL**: Enable POSIX ACL support (if supported by kernel
  and underlying filesystem). (default: false)
- **async_read=BOOL**: Perform reads asynchronously. If disabled or
  unavailable the kernel will ensure there is at most one pending read
  request per file handle and will attempt to order requests by
  offset. (default: true)
- **fuse_msg_size=UINT**: Set the max number of pages per FUSE
  message. Only available on Linux >= 4.20 and ignored
  otherwise. (min: 1; max: 256; default: 256)
- **threads=INT**: Number of threads to use. When used alone
  (`process-thread-count=-1`) it sets the number of threads reading
  and processing FUSE messages. When used together it sets the number
  of threads reading from FUSE. When set to zero it will attempt to
  discover and use the number of logical cores. If the thread count is
  set negative it will look up the number of cores then divide by the
  absolute value. ie. threads=-2 on an 8 core machine will result in 8
  / 2 = 4 threads. There will always be at least 1 thread. If set to
  -1 in combination with `process-thread-count` then it will try to
  pick reasonable values based on CPU thread count. NOTE: higher
  number of threads increases parallelism but usually decreases
  throughput. (default: 0)
- **read-thread-count=INT**: Alias for `threads`.
- **process-thread-count=INT**: Enables separate thread pool to
  asynchronously process FUSE requests. In this mode
  `read-thread-count` refers to the number of threads reading FUSE
  messages which are dispatched to process threads. -1 means disabled
  otherwise acts like `read-thread-count`. (default: -1)
- **process-thread-queue-depth=UINT**: Sets the number of requests any
  single process thread can have queued up at one time. Meaning the
  total memory usage of the queues is queue depth multiplied by the
  number of process threads plus read thread count. 0 sets the depth
  to the same as the process thread count. (default: 0)
- **pin-threads=STR**: Selects a strategy to pin threads to CPUs
  (default: unset)
- **flush-on-close=never|always|opened-for-write**: Flush data cache
  on file close. Mostly for when writeback is enabled or merging
  network filesystems. (default: opened-for-write)
- **scheduling-priority=INT**: Set mergerfs' scheduling
  priority. Valid values range from -20 to 19. See `setpriority` man
  page for more details. (default: -10)
- **fsname=STR**: Sets the name of the filesystem as seen in
  **mount**, **df**, etc. Defaults to a list of the source paths
  concatenated together with the longest common prefix removed.
- **func.FUNC=POLICY**: Sets the specific FUSE function's policy. See
  below for the list of value types. Example: **func.getattr=newest**
- **func.readdir=seq|cosr|cor|cosr:INT|cor:INT**: Sets `readdir`
  policy. INT value sets the number of threads to use for
  concurrency. (default: seq)
- **category.action=POLICY**: Sets policy of all FUSE functions in the
  action category. (default: epall)
- **category.create=POLICY**: Sets policy of all FUSE functions in the
  create category. (default: epmfs)
- **category.search=POLICY**: Sets policy of all FUSE functions in the
  search category. (default: ff)
- **cache.statfs=UINT**: 'statfs' cache timeout in seconds. (default: 0)
- **cache.attr=UINT**: File attribute cache timeout in
  seconds. (default: 1)
- **cache.entry=UINT**: File name lookup cache timeout in
  seconds. (default: 1)
- **cache.negative_entry=UINT**: Negative file name lookup cache
  timeout in seconds. (default: 0)
- **cache.files=libfuse|off|partial|full|auto-full|per-process**: File
  page caching mode (default: libfuse)
- **cache.files.process-names=LIST**: A pipe | delimited list of
  process [comm](https://man7.org/linux/man-pages/man5/proc.5.html)
  names to enable page caching for when
  `cache.files=per-process`. (default: "rtorrent|qbittorrent-nox")
- **cache.writeback=BOOL**: Enable kernel writeback caching (default:
  false)
- **cache.symlinks=BOOL**: Cache symlinks (if supported by kernel)
  (default: false)
- **cache.readdir=BOOL**: Cache readdir (if supported by kernel)
  (default: false)
- **parallel-direct-writes=BOOL**: Allow the kernel to dispatch
  multiple, parallel (non-extending) write requests for files opened
  with `cache.files=per-process` (if the process is not in `process-names`)
  or `cache.files=off`. (This requires kernel support, and was added in v6.2)

**NOTE:** Options are evaluated in the order listed so if the options
are **func.rmdir=rand,category.action=ff** the **action** category
setting will override the **rmdir** setting.

**NOTE:** Always look at the documentation for the version of mergerfs
you're using. Not all features are available in older releases.

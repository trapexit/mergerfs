# Options Test Plan

Possible integration and unit tests for options listed in `mkdocs/docs/config/options.md`.

Legend:
- **Integration**: mount-level behavior test (black-box, syscall parity / side effects)
- **Unit**: focused parser/config/logic test (white-box or low-level component)

## Core mount and branch options

| Option | Integration tests | Unit tests |
|---|---|---|
| `config` | Load same settings via CLI vs config file and compare behavior | Parse comments, `key`, `key=val`, unknown keys, duplicate keys ordering |
| `branches` | Multi-branch create/search/action behavior; glob expansion; dynamic branch updates | Branch string parser ops (`+`, `+<`, `+>`, `-`, `->`, `-<`, `=`), mode parse, minfreespace parse |
| `mountpoint` | Verify mount succeeds/fails with valid/invalid mountpoint | Config validation for mountpoint path |
| `branches-mount-timeout` | Delayed branch mount appears before timeout -> starts; after timeout -> continue/fail based on flag | Timeout arithmetic and mount-detection helper logic |
| `branches-mount-timeout-fail` | Timeout expiration returns non-zero when true | Boolean parse and gating of startup continuation |
| `minfreespace` | Create policy excludes near-full branches | Size parser (`B/K/M/G/T`, overflow, bad suffix) |
| `moveonenospc` | ENOSPC write triggers move+retry and preserves metadata | Policy dispatch and error mapping (`ENOSPC`/`EDQUOT`) |
| `inodecalc` | Stable inode expectations per mode on files/dirs/hardlinks | Hash function determinism and 32-bit/64-bit variants |

## IO behavior options

| Option | Integration tests | Unit tests |
|---|---|---|
| `dropcacheonclose` | Close after write/read and confirm no correctness regression | `posix_fadvise` call path invoked only when enabled |
| `direct-io-allow-mmap` | `O_DIRECT` + `mmap` behavior on supported kernels | Feature flag gating by kernel capability |
| `nullrw` | Writes report success but data unchanged; reads become no-op behavior | Read/write null path selection |
| `readahead` | Sequential read throughput/request size changes with non-zero setting | Parser and branch-level readahead setter behavior |
| `async-read` | Concurrent read ordering/parallelism differs when toggled | FUSE config flag toggling |
| `fuse-msg-size` | Large IO request chunking and throughput with different values | Pagesize conversion parser and min/max clamping |
| `flush-on-close` | `never/always/opened-for-write` close semantics | Mode enum parse and branching logic |
| `proxy-ioprio` | Different caller ioprio reflected in IO scheduling side-effects | ioprio fetch/apply path and fallback behavior |
| `parallel-direct-writes` | Parallel non-extending writes behavior with/without option | Kernel capability gating |
| `passthrough.io` | `off/ro/wo/rw` path selection for reads/writes and incompatibility checks | Enum parse, compatibility checks (`cache.writeback`, `nullrw`, etc.) |
| `passthrough.max-stack-depth` | Stacked FS scenarios with depth 1 vs 2 | Range validation and config propagation |

## Path transformation and cross-device options

| Option | Integration tests | Unit tests |
|---|---|---|
| `symlinkify` | Old non-writable files appear as symlinks after timeout | Eligibility predicate (mode, age, type) |
| `symlinkify-timeout` | Boundary test around exact timeout second | Timeout comparison helper |
| `ignorepponrename` | Rename behavior with and without path preservation | Rename policy selection logic |
| `follow-symlinks` | `never/directory/regular/all` on symlink to file/dir/broken link | Enum parse and target-type dispatch |
| `link-exdev` | EXDEV hardlink fallback modes: passthrough/rel/abs-base/abs-pool | Strategy selection and symlink target synthesis |
| `rename-exdev` | EXDEV rename fallback modes: passthrough/rel/abs | Fallback path generation and state machine |
| `link-cow` | Hardlinked file opened for write breaks link and writes private copy | Link-count check and copy-on-write trigger logic |

## Permission and metadata options

| Option | Integration tests | Unit tests |
|---|---|---|
| `export-support` | NFS-export style behavior sanity and mount flags exposure | Init-time flag propagation |
| `kernel-permissions-check` | Access/permission checks handled by kernel vs userspace path | Mount option mapping (`default_permissions`) |
| `security-capability` | `security.capability` xattr returns ENOATTR when disabled | Name filter helper |
| `xattr` | `passthrough/noattr/nosys` behavior for set/get/list/remove xattr | Enum parse and short-circuit return paths |
| `statfs` | `base` vs `full` branch inclusion by path | Mode parse and branch-filter helper |
| `statfs-ignore` | `none/ro/nc` effect on available space accounting | Ignore predicate and accumulator logic |
| `nfsopenhack` | File create/open mode behavior under NFS-like patterns | Enum parse and hook decision logic |
| `posix-acl` | ACL xattr roundtrip and chmod/chown interaction when enabled | Mount/init flag propagation |

## Threading and scheduling options

| Option | Integration tests | Unit tests |
|---|---|---|
| `read-thread-count` | Throughput/latency and correctness under different values | Value normalization (`0`, negatives, min=1) |
| `process-thread-count` | Separation of read/process pools and queue pressure behavior | Pool enable/disable logic |
| `process-thread-queue-depth` | Backpressure behavior with depth changes | Queue sizing and timeout/try-enqueue semantics |
| `pin-threads` | Thread CPU affinity placement for each strategy | Strategy parser and CPU selection algorithm |
| `scheduling-priority` | Process nice value set at startup and effective scheduling | Range validation and setpriority wrapper |

## Function/category policy options

| Option | Integration tests | Unit tests |
|---|---|---|
| `func.FUNC` | Override a single function policy and verify only that op changes | Parser mapping from key to policy holder |
| `func.readdir` | `seq/cosr/cor` ordering, latency, duplicate suppression, thread count parsing | Mode parser (`cosr:N:M`, `cor:N:M`) |
| `category.action` | Bulk action policy changes for chmod/chown/rename/unlink | Category-to-function propagation |
| `category.create` | Create policy effects on create/mkdir/mknod/link/symlink | Category propagation and precedence |
| `category.search` | Search policy effects on getattr/open/readlink/getxattr | Category propagation and precedence |
| Option ordering note | Later options override earlier (`func.*` then `category.*` etc.) | Deterministic application order test |

## Cache options

| Option | Integration tests | Unit tests |
|---|---|---|
| `cache.statfs` | Repeated statfs calls use cache within timeout | Timeout cache invalidation behavior |
| `cache.attr` | Repeated getattr freshness across timeout boundaries | Attr cache key and expiry logic |
| `cache.entry` | Name lookup caching for existing entries | Entry cache insertion/expiry |
| `cache.negative-entry` | Missing entry lookup caching and stale-negative behavior | Negative cache behavior |
| `cache.files` | `off/partial/full/auto-full/per-process` correctness and coherence | Enum parse and mode decision helper |
| `cache.files.process-names` | Per-process cache activation by comm name | List parser and matcher |
| `cache.writeback` | Coalesced write behavior; incompatibility handling with passthrough | Flag validation and FUSE config setup |
| `cache.symlinks` | Readlink repeated calls reflect cache policy | Feature gating by kernel support |
| `cache.readdir` | Readdir repeated calls and invalidation behavior | Feature gating and config wiring |

## Misc options

| Option | Integration tests | Unit tests |
|---|---|---|
| `lazy-umount-mountpoint` | Live-upgrade remount scenario with stale previous mount | Startup control flow and error handling |
| `remember-nodes` | NFS-like lookup stability until unlink/remove/rename replacement | Boolean and numeric compatibility parsing |
| `noforget` | Retained nodes released on unlink/remove/rename replacement | Alias mapping to retained-node mode |
| `allow-idmap` | UID/GID mapping behavior in idmapped mounts | Init flag propagation and compatibility checks |
| `debug` | FUSE trace generation enabled/disabled | Debug option parse and logger toggling |
| `log.file` | Trace output to stderr vs file path | Path parser and file sink setup |
| `fsname` | Filesystem name shown in mount/df matches config | fsname default derivation and override |

## Prioritized backlog (recommended first)

1. `xattr` mode tests (`passthrough/noattr/nosys`) + regression for current xattr mismatch
2. `statfs` / `statfs-ignore` mode matrix
3. EXDEV fallbacks (`link-exdev`, `rename-exdev`)
4. `follow-symlinks` + `symlinkify` behavior matrix
5. Policy precedence (`func.*`, `category.*`, ordering)
6. Cache timeout behavior (`cache.attr`, `cache.entry`, `cache.negative-entry`, `cache.statfs`)

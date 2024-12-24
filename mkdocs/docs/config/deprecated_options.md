# Deprecated Options

These are old, deprecated options which may no longer have any
function or have been replaced.

* **direct_io**: Bypass page cache. Use `cache.files=off`
  instead.
* **kernel_cache**: Do not invalidate data cache on file open. Use
  `cache.files=full` instead.
* **auto_cache**: Invalidate data cache if file mtime or
  size change. Use `cache.files=auto-full` instead. (default: false)
* **async_read**: Perform reads asynchronously. Use
  `async_read=true` instead.
* **sync_read**: Perform reads synchronously. Use
  `async_read=false` instead.
* **splice_read**: Does nothing.
* **splice_write**: Does nothing.
* **splice_move**: Does nothing.
* **allow_other**: mergerfs v2.35.0 and above sets this FUSE option
  automatically if running as root.
* **use_ino**: Effectively replaced with `inodecalc`.

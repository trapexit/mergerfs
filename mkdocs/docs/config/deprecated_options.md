# Deprecated Options

These are old, deprecated options which may no longer have any
function or have been replaced. **They should not be used.**

* **allow_other**: mergerfs v2.35.0 and above sets this FUSE option
  automatically if running as root.
* **async_read**: Use `async_read=true`.
* **atomic_o_trunc**: Does nothing.
* **big_writes**: Does nothing.
* **attr_timeout**: Use `cache.attr`.
* **auto_cache**: Use `cache.files=auto-full`.
* **direct_io**: Use `cache.files=off`.
* **entry_timeout**: Use `cache.entry`.
* **hard_remove**: Does nothing.
* **kernel_cache**: Use `cache.files=full`.
* **negative_entry**: Use `cache.negative_entry`.
* **nonempty**: Does nothing.
* **splice_move**: Does nothing.
* **splice_read**: Does nothing.
* **splice_write**: Does nothing.
* **sync_read**: Use `async_read=false`.
* **use_ino**: Use `inodecalc`.

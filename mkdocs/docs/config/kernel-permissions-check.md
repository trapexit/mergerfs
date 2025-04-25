# kernel-permissions-check

* `kernel-permissions-check=true|false`
* Default: `true`


[FUSE](https://www.kernel.org/doc./html/next/filesystems/fuse.html)
has a feature which mergerfs leverages which allows the kernel to do
file permission checking rather than leaving it to the FUSE server (in
this case mergerfs.) This improves performance. However, it also
limits flexibility.

mergerfs should work fine regardless of this setting but there might
be some currently unknown edge cases where disabling the feature might
help. Like [export-support](export-support.md) this is mostly for
debugging.

This option is a kernel mount option so unable to be changed at
runtime.

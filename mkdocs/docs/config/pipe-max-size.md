# pipe-max-size

* type: `SIZE`
* default: `0`
* example: `pipe-max-size=8M`

Sets the value written to `/proc/sys/fs/pipe-max-size` at mount time,
before the FUSE connection is negotiated. This is the kernel's limit
on the capacity of a single pipe, which bounds how much data can be
moved in a single `splice` call.

mergerfs already raises this sysctl on its own, automatically,
whenever splice is enabled (the default) and `fuse-msg-size` doesn't
fit the current `pipe-max-size` (default 1MiB on most kernels) - that
automatic behavior does not depend on this option being set. Setting
`pipe-max-size` explicitly is for cases where you want the sysctl
raised to a specific value up front (e.g. more headroom than the
minimum mergerfs would otherwise request), or want it raised before
mergerfs starts negotiating rather than during. If you use the
default `fuse-msg-size` you do not need this option.

Writing the sysctl requires running as root. When not root mergerfs
will log (at info level) that it could not raise the limit and will
clamp `fuse-msg-size` to what the current pipe limit allows instead.

Note that this is a system wide setting that persists after mergerfs
exits; mergerfs does not restore the previous value on unmount.

For unprivileged usage the kernel also caps the total number of pipe
pages a non-root user may have allocated, via
`/proc/sys/fs/pipe-user-pages-soft` and
`/proc/sys/fs/pipe-user-pages-hard` (both present since Linux
v4.5). This is a separate limit from `pipe-max-size`, which caps a
single pipe's capacity.

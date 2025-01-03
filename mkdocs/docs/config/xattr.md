# xattr

* `xattr=passthrough`: Passes through all requests to underlying file.
* `xattr=noattr`: mergerfs receives the request but returns `NOATTR`.
* `xattr=nosys`: Tells the kernel to reject all `xattr` requests.
* Defaults to `passthrough`.

Runtime extended attribute support can be managed via the `xattr`
option. By default it will passthrough any xattr calls. Given xattr
support is rarely used and can have significant performance
implications mergerfs allows it to be disabled at runtime. The
performance problems mostly comes when file caching is enabled. The
kernel will send a `getxattr` for `security.capability` *before every
single write*. It doesn't cache the responses to any `getxattr`. This
might be addressed in the future but for now mergerfs can really only
offer the following workarounds.

`noattr` will cause mergerfs to short circuit all xattr calls and
return ENOATTR where appropriate. mergerfs still gets all the requests
but they will not be forwarded on to the underlying filesystems. The
runtime control will still function in this mode.

`nosys` will cause mergerfs to return `ENOSYS` for any xattr call. The
difference with `noattr` is that the kernel will cache this fact and
itself short circuit future calls. This is more efficient than
`noattr` but will cause mergerfs' runtime control via the hidden file
to stop working.

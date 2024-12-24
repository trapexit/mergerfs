# export-support

* `export-support=true|false`
* Defaults to `true`.

In theory, this flag should not be exposed to the end user. It is a
low-level FUSE flag which indicates whether or not the kernel can send
certain kinds of messages to it for the purposes of using it with
NFS. mergerfs does support these messages but due to bugs and quirks
found in the kernel and mergerfs this option is provided just in case
it is needed for debugging.

Given that this flag is set when the FUSE connection is first
initiated it is not possible to change during run time.

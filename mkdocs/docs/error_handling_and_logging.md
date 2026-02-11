# Error Handling and Logging

## Error Handling

POSIX filesystem functions typically work on singular items and return
singular errors. This means that there is some complication when
dealing with multiple files or behaviors that are required in mergerfs.

mergerfs tries to handle errors in a way that would generally return
meaningful values for that particular function.

### Filesystem Functions

#### chmod, chown, removexattr, setxattr, truncate, utimens

1. if no errors: return 0 (success)
2. if no successes: return first error
3. if one of the files acted on was the same as the related search
   function: return its value
4. return 0 (success)

While doing this increases the complexity and cost of error handling,
particularly step 3, this provides probably the most reasonable return
value.


#### unlink, rmdir

1. if no errors: return 0 (success)
2. return first error

Older versions of mergerfs would return success if any success
occurred but for unlink and rmdir there are downstream assumptions
that, while not impossible to occur in more traditional situation, can
confuse some software.

#### open, create

For `create` and `open` where a single file is targeted the return
value of the equivalent final call is returned.

If the error returned is `EROFS`, indicating the filesystem is in fact
read-only, mergerfs will mark the branch `RO` and rerun the
policy. This typically will only happen with `ext4` when the
filesystem is found to have corruption and the OS remounts the
filesystem as read-only to protect it. In that situation the
filesystem does not otherwise indicate that it is read-only as it
would if mounted with `ro`.


#### others

For search functions, there is always a single thing acted on and as
such whatever return value that comes from the single function call is
returned.

For create functions `mkdir`, `mknod`, and `symlink` which don't
return a file descriptor and therefore can have `all` or `epall`
policies it will return success if any of the calls succeed and an
error otherwise.


## Branches disappearing / reappearing

This is not an error condition. mergerfs works on paths. Not
mounts. There is currently no assumption or active inspection of the
branch path provided at runtime. Nor does it keep its own open file
descriptors that would prevent an unused filesystem from being
unmounted. If a filesystem disappears for any reason mergerfs will
simply continue on behaving as it would normally. As if you never
mounted that filesystem into that location. If a branch path no longer
exists the branch is simply skipped by policies.

If you wish to keep the branch path from being used when a branch
path's intended filesystem disappears then make the directory
difficult or impossible to use.

```
chown root:root /mnt/mountpoint/
chmod 0000 /mnt/mountpoint/
chattr +i /mnt/mountpoint/
```

You can still mount to that directory but you will not be able to
write to it. Even as root. Note that `chattr` works on limited
filesystems. Mainly `ext4`.


## Logging

Filesystems, and therefore mergerfs, are doing **lots** of small
actions at high speed. It simply isn't reasonable to log all the
actions of the system all time time. However, there is a debug mode
which can be toggled on as needed, even at runtime, which will record
some information to help with debugging. The main thing it will record
is a trace of all FUSE messages to the location defined by the
`log.file` option. See [config section](config/options.md) for more
details.

That said even with debug mode disabled mergerfs will log certain
details via `syslog` which on `systemd` based systems can be viewed by
running:

```
journalctl -t mergerfs
```

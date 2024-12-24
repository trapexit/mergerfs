# branches

The 'branches' argument is a colon (':') delimited list of paths to be
pooled together. It does not matter if the paths are on the same or
different filesystems nor does it matter the filesystem type (within
reason). Used and available space will not be duplicated for paths on
the same filesystem and any features which aren't supported by the
underlying filesystem (such as file attributes or extended attributes)
will return the appropriate errors.

Branches currently have two options which can be set. A type which
impacts whether or not the branch is included in a policy calculation
and a individual minfreespace value. The values are set by prepending
an `=` at the end of a branch designation and using commas as
delimiters. Example: `/mnt/drive=RW,1234`

### branch mode

- RW: (read/write) - Default behavior. Will be eligible in all policy
  categories.
- RO: (read-only) - Will be excluded from `create` and `action`
  policies. Same as a read-only mounted filesystem would be (though
  faster to process).
- NC: (no-create) - Will be excluded from `create` policies. You can't
  create on that branch but you can change or delete.

### minfreespace

Same purpose and syntax as the global option but specific to the
branch. If not set the global value is used.

### globbing

To make it easier to include multiple branches mergerfs supports
[globbing](http://linux.die.net/man/7/glob). **The globbing tokens
MUST be escaped when using via the shell else the shell itself will
apply the glob itself.**

```
# mergerfs /mnt/hdd\*:/mnt/ssd /media
```

The above line will use all mount points in /mnt prefixed with **hdd**
and **ssd**.

To have the pool mounted at boot or otherwise accessible from related
tools use `/etc/fstab`.

```
# <file system>        <mount point>  <type>    <options>             <dump>  <pass>
/mnt/hdd*:/mnt/ssd    /media          mergerfs  minfreespace=16G      0       0
```

**NOTE:** The globbing is done at mount or when updated using the runtime API. If a new directory is added matching the glob after the fact it will not be automatically included.

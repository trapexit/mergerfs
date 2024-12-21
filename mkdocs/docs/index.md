# mergerfs - a featureful union filesystem

## DESCRIPTION

**mergerfs** is a union filesystem geared towards simplifying storage
and management of files across numerous commodity storage devices. It
is similar to **mhddfs**, **unionfs**, and **aufs**.

## FEATURES

- Configurable behaviors / file placement
- Ability to add or remove filesystems at will
- Resistance to individual filesystem failure
- Support for extended attributes (xattrs)
- Support for file attributes (chattr)
- Runtime configurable (via xattrs)
- Works with heterogeneous filesystem types
- Moving of file when filesystem runs out of space while writing
- Ignore read-only filesystems when creating files
- Turn read-only files into symlinks to underlying file
- Hard link copy-on-write / CoW
- Support for POSIX ACLs
- Misc other things

## SYNOPSIS

mergerfs -o&lt;options&gt; &lt;branches&gt; &lt;mountpoint&gt;

## DOCUMENTATION

- [https://trapexit.github.io/mergerfs/](https://trapexit.github.io/mergerfs/)

## TOOLS

- [mergerfs tools](https://github.com/trapexit/mergerfs-tools)

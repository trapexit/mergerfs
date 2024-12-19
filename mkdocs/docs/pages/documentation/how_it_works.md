# HOW IT WORKS

mergerfs logically merges multiple paths together. Think a union of
sets. The file/s or directory/s acted on or presented through mergerfs
are based on the policy chosen for that particular action. Read more
about policies below.

```
A         +      B        =       C
/disk1           /disk2           /merged
|                |                |
+-- /dir1        +-- /dir1        +-- /dir1
|   |            |   |            |   |
|   +-- file1    |   +-- file2    |   +-- file1
|                |   +-- file3    |   +-- file2
+-- /dir2        |                |   +-- file3
|   |            +-- /dir3        |
|   +-- file4        |            +-- /dir2
|                     +-- file5   |   |
+-- file6                         |   +-- file4
                                  |
                                  +-- /dir3
                                  |   |
                                  |   +-- file5
                                  |
                                  +-- file6
```

mergerfs does **not** support the copy-on-write (CoW) or whiteout
behaviors found in **aufs** and **overlayfs**. You can **not** mount a
read-only filesystem and write to it. However, mergerfs will ignore
read-only filesystems when creating new files so you can mix
read-write and read-only filesystems. It also does **not** split data
across filesystems. It is not RAID0 / striping. It is simply a union of
other filesystems.

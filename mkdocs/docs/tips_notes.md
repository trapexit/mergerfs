# Tips and Notes

* This document is literal and reasonably thorough. If a suspected
  feature isn't mentioned it doesn't exist. If certain `libfuse`
  arguments aren't listed they probably shouldn't be used.
* Ensure you're using the latest version. Especially before submitting
  bug reports.
* Run mergerfs as `root`. mergerfs is designed and intended to be run
  as `root` and may exhibit incorrect behavior if run otherwise.
* If you do not see some directories and files you expect, policies
  seem to skip branches, you get strange permission errors, etc. be
  sure the underlying filesystems' permissions are all the same. Use
  `mergerfs.fsck` to audit the filesystem for out of sync permissions.
* If you still have permission issues be sure you are using POSIX ACL
  compliant filesystems. mergerfs doesn't generally make exceptions
  for FAT, NTFS, or other non-POSIX filesystem.
* Unless using Linux v6.6 or above do **not** use `cache.files=off` if
  you expect applications (such as rtorrent) to use
  [mmap](http://linux.die.net/man/2/mmap). Enabling `dropcacheonclose`
  is recommended when `cache.files=auto-full`.
* [Kodi](http://kodi.tv), [Plex](http://plex.tv),
  [Subsonic](http://subsonic.org), etc. can use directory
  [mtime](http://linux.die.net/man/2/stat) to more efficiently
  determine whether to scan for new content rather than simply
  performing a full scan. If using the default `getattr` policy of
  `ff` it's possible those programs will miss an update on account of
  it returning the first directory found's `stat` info and it is a
  later directory on another mount which had the `mtime` recently
  updated. To fix this you will want to set
  `func.getattr=newest`. Remember though that this is just `stat`. If
  the file is later `open`'ed or `unlink`'ed and the policy is
  different for those then a completely different file or directory
  could be acted on.
* Some policies mixed with some functions may result in strange
  behaviors. Not that some of these behaviors and race conditions
  couldn't happen outside mergerfs but that they are far more
  likely to occur on account of the attempt to merge multiple sources
  of data which could be out of sync due to the different policies.
* For consistency it's generally best to set `category` wide policies
  rather than individual `func`'s. This will help limit the
  confusion of tools such as
  [rsync](http://linux.die.net/man/1/rsync). However, the flexibility
  is there if needed.

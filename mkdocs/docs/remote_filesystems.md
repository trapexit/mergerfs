# Remote Filesystems

This section is to describe any known issues or quirks when using
mergerfs with common remote filesystems.

There are two ways to use mergerfs with a network filesystem. One is
to use it within the pool as a branch and one is the exporting of a
mergerfs mount.


## General Notes

### Do not change things out-of-band

Keep in mind that, like when using
[caching](faq/usage_and_functionality.md#can-filesystems-still-be-used-directly-outside-of-mergerfs-while-pooled),
it is **NOT** a good idea to change the contents of the remote
filesystem
[out-of-band](https://en.wikipedia.org/wiki/Out-of-band). Meaning that
you should not change the contents of the underlying filesystems or
mergerfs on the server hosting the remote filesystem. Files should
exclusively be interacted with through the network filesystem. Doing
otherwise can lead to weird behavior, inconsistency, errors, and even
data corruption should multiple programs try to write or read the same
data at the same time. This isn't to say you can't do it or that data
corruption is likely but it _could_ happen. If you're only reading
from those filesystems there is little risk but writing or modifying
file layout will be a problem. Particularly with NFS. It is better to
always use the remote filesystem. Even on the machine serving it.


## NFS

[NFS](https://en.wikipedia.org/wiki/Network_File_System) is a common
remote filesystem on Unix/POSIX systems. Due to how NFS works there
are some settings which need to be set in order for mergerfs to work
with it.

### NFS as branch

mergerfs settings:

* No special settings need to be set

NFS export settings:

* `no_root_squash`: mergerfs must be able to make requests that only
  root is permissioned to make. If any root squashing occurs, just as
  running mergerfs as a non-root user, there is no guarantee that
  mergerfs will be able to do what it needs to manage the underlying
  filesystem.
* `softerr` or `soft`: A perhaps questionable choice but if NFS
  freezes up because the backend dies or the network goes down the
  default `hard` mount would cause mergerfs to block the same as any
  other software. By setting `softerr` or `soft` the NFS client will
  timeout eventually and return an error.
* `softreval`: NFS client will serve up cached data after `retrans`
  attempts to revalidate the data. Helps with intermitent network
  issues.
* `timeo=150`: Timeout till retrying request. 
* `retrans=3`: Number of retrying a request.
* `rsize=131072`: Read size.
* `wsize=131072`: Write size.

`no_root_squash` is necessary but the others are just suggestions and
you may want to alter based on your use case. The risk of using `hard`
and other more strict behavior options is that should NFS completely
lock up it will impact mergerfs negatively.


### NFS exporting mergerfs

It should be noted that NFS and FUSE (the technology mergerfs uses) do
not work perfectly with one another. They largely can be worked around
but if you run into problems it may be worth trying Samba/SMB.

**mergerfs settings:**

* `noforget`
* `inodecalc=path-hash`
* `lazy-umount-mountpoint=false`

`noforget` is needed because NFS uses the `name_to_handle_at` and
`open_by_handle_at` functions which allow a program to keep a
reference to a file without technically having it open in the typical
sense. The problem is that FUSE has no way to know that NFS has a
handle that it will later use to open the file again. As a result, it
is possible for the kernel to tell mergerfs to forget about the file
node and should NFS ever ask for that node's details in the future it
would have nothing to respond with. Keeping nodes around forever is
not ideal but at the moment the only way to manage the situation.

`inodecalc=path-hash` is needed because NFS is sensitive to
out-of-band changes. FUSE doesn't care if a file's inode value changes
but NFS, being stateful, does. So if you used the default inode
calculation algorithm it is possible that if you changed a file or
updated a directory the file mergerfs will use will be on a different
branch and therefore the inode would change. This isn't an ideal
solution and others are being considered but it works for most
situations.

`lazy-umount-mountpoint=false` is needed (or left unset) as lazy
umount can cause problems with NFS. It won't impact the NFS export at
first but followup mounts of mergerfs to the same mount point will
lead to new remote mounts hanging till `exportfs` is run again.


**NFS export settings:**

* `fsid=UUID`
* `no_root_squash`


`fsid=UUID` is needed because FUSE filesystems don't have different
`st_dev` values which can cause issues when exporting. The easiest
thing to do is set each mergerfs export `fsid` to some random
value. An easy way to generate a random value is to use the command
line tool `uuid` or `uuidgen` or through a website such as
[uuidgenerator.net](https://www.uuidgenerator.net/).

`no_root_squash` is required for the same reason mergerfs needs to run
as `root`. Certain behaviors of mergerfs require control over the
filesystem that only `root` can preform. If squashing is enabled, or
mergerfs was running as non-root, it would be unable to perform
certain function and you will receive permission errors.


## SMB / CIFS

[SMB](https://en.wikipedia.org/wiki/Server_Message_Block) is a
protocol most used by Microsoft Windows systems to share file shares,
printers, etc. However, due to the popularity of Windows, it is also
supported on many other platforms including Linux. The most popular
way of supporting SMB on Linux is via the software Samba.

### SMB as branch

Using a SMB mount as a branch in mergerfs may result in problems
(permission errors) since it is not POSIX compliant. This setup is not
common and has not been extensively tested.


### SMB exporting mergerfs

[Samba](<https://en.wikipedia.org/wiki/Samba_(software)>) and other
ways of serving Linux filesystems via SMB should work fine with
mergerfs. The services do not tend to use the same technologies which
NFS uses and therefore don't have the same issues. There should not be
special settings required export mergerfs with Samba. However,
[CIFSD](https://en.wikipedia.org/wiki/CIFSD) and other programs have
not been extensively tested. If you use mergerfs with CIFSD or other
SMB servers please submit your experiences so these docs can be
updated.

NOTE: Some users have reported that when deleting files from certain
Android file managers they get errors from the app and Samba but the
file gets removed. This is due to a bug in Samba which has been fixed
in more recent releases such as 4.21. If using Debian you can enable
[backports repo](https://backports.debian.org/Instructions).


## SSHFS

[SSHFS](https://en.wikipedia.org/wiki/SSHFS) is a FUSE filesystem
leveraging SSH as the connection and transport layer. While often
simpler to setup when compared to NFS or Samba the performance can be
lacking and the project is very much in maintenance mode.

There are no known issues using sshfs with mergerfs. You may want to
use the following arguments to improve performance but your millage
may vary.

* `-o Ciphers=arcfour`
* `-o Compression=no`

More info can be found
[here](https://ideatrash.net/2016/08/odds-and-ends-optimizing-sshfs-moving.html).


## Other

There are other remote filesystems but none popularly used to serve
mergerfs. If you use something not listed above feel free to reach out
and I will add it to the list.

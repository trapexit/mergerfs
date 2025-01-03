# Remote Filesystems

Many users ask about compatibility with remote filesystems. This
section is to describe any known issues or quirks when using mergerfs
with common remote filesystems.

Keep in mind that, like with caching, it is **NOT** a good idea to
change the contents of the remote filesystem
[out-of-band](https://en.wikipedia.org/wiki/Out-of-band). Meaning that
you should not change the contents of the underlying filesystems or
mergerfs on the server hosting the remote filesystem. Doing so can
lead to weird behavior, inconsistency, errors, and even data
corruption should multiple programs try to write or read the same data
at the same time. This isn't to say you can't do it or that data
corruption is likely but it _could_ happen. It is better to always use
the remote filesystem. Even on the machine serving it.

## NFS

[NFS](https://en.wikipedia.org/wiki/Network_File_System) is a common
remote filesystem on Unix/POSIX systems. Due to how NFS works there
are some settings which need to be set in order for mergerfs to work
with it.

It should be noted that NFS and FUSE (the technology mergerfs uses) do
not work perfectly with one another due to certain design choices in
FUSE (and mergerfs.) Due to these issues, it is generally recommended
to use SMB when possible. That said mergerfs should generally work as
an export of NFS and issues discovered should still be reported.

To ensure compatibility between mergerfs and NFS use the following
settings.

mergerfs settings:

* `noforget`
* `inodecalc=path-hash`

NFS export settings:

* `fsid=UUID`
* `no_root_squash`

`noforget` is needed because NFS uses the `name_to_handle_at` and
`open_by_handle_at` functions which allow a program to keep a
reference to a file without technically having it open in the typical
sense. The problem is that FUSE has no way to know that NFS has a
handle that it will later use to open the file again. As a result, it
is possible for the kernel to tell mergerfs to forget about the node
and should NFS ever ask for that node's details in the future it would
have nothing to respond with. Keeping nodes around forever is not
ideal but at the moment the only way to manage the situation.

`inodecalc=path-hash` is needed because NFS is sensitive to
out-of-band changes. FUSE doesn't care if a file's inode value changes
but NFS, being stateful, does. So if you used the default inode
calculation algorithm then it is possible that if you changed a file
or updated a directory the file mergerfs will use will be on a
different branch and therefore the inode would change. This isn't an
ideal solution and others are being considered but it works for most
situations.

`fsid=UUID` is needed because FUSE filesystems don't have different
`st_dev` values which can cause issues when exporting. The easiest
thing to do is set each mergerfs export `fsid` to some random
value. An easy way to generate a random value is to use the command
line tool `uuid` or `uuidgen` or through a website such as
[uuidgenerator.net](https://www.uuidgenerator.net/).

`no_root_squash` is not strictly necessary but can lead to confusing
permission and ownership issues if root squashing is enabled. mergerfs
needs to run certain commands as `root` and if root squash is enabled
it NFS will tell mergerfs a non-root user is making certain calls.


## SMB / CIFS

[SMB](https://en.wikipedia.org/wiki/Server_Message_Block) is a
protocol most used by Microsoft Windows systems to share file shares,
printers, etc. However, due to the popularity of Windows, it is also
supported on many other platforms including Linux. The most popular
way of supporting SMB on Linux is via the software Samba.

[Samba](<https://en.wikipedia.org/wiki/Samba_(software)>), and other
ways of serving Linux filesystems, via SMB should work fine with
mergerfs. The services do not tend to use the same technologies which
NFS uses and therefore don't have the same issues. There should not be
special settings required to use mergerfs with Samba. However,
[CIFSD](https://en.wikipedia.org/wiki/CIFSD) and other programs have
not been extensively tested. If you use mergerfs with CIFSD or other
SMB servers please submit your experiences so these docs can be
updated.


## SSHFS

[SSHFS](https://en.wikipedia.org/wiki/SSHFS) is a FUSE filesystem
leveraging SSH as the connection and transport layer. While often
simpler to setup when compared to NFS or Samba the performance can be
lacking and the project is very much in maintenance mode.

There are no known issues using sshfs with mergerfs. You may want to
use the following arguments to improve performance but your millage
may vary.

- `-o Ciphers=arcfour`
- `-o Compression=no`

More info can be found
[here](https://ideatrash.net/2016/08/odds-and-ends-optimizing-sshfs-moving.html).


## Other

There are other remote filesystems but none popularly used to serve
mergerfs. If you use something not listed above feel free to reach out
and I will add it to the list.

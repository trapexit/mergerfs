# Limiting drive spinup

## How can I setup my system to limit drive spinup?

TL;DR: You really can't. Not through mergerfs alone.


mergerfs is a proxy. Not a cache. It proxies calls between client software and underlying filesystems. If a client does an `open`, `readdir`, `stat`, etc. it must translate that into something that makes sense across N filesystems. For `readdir` that means running the call against all branches and aggregating the output. For `open` that means finding the file to open and doing so. The only way to find the file to open is to scan across all branches and sort the results and pick one. There is no practical way to do otherwise. Especially given so many mergerfs users expect out of band changes to "just work."

The best way to limit spinup of drives is to limit their usage at the client level. Meaning keeping software from interacting with the filesystem all together.

### What if you assume no out of band changes and cache everything?

This would require a significant rewrite of mergerfs. Everything is done on the fly right now and all those calls to underlying filesystems can cause a spinup. To work around that a database of some sort would have to be used to store ALL metadata about the underlying filesystems and on startup everything scanned and stored. From then on it would have to carefully update all the same data the filesystems do. It couldn't be kept in RAM because it would take up too much space so it'd have to be on a SSD or other storage device. If anything changed out of band it would break things in weird ways. It could rescan on occasion but that would require spinning up everything. It could put file watches on every single directory but that probably won't scale (there are millions of directories in my system for example) and the open files might keep the drives from spinning down. Something as "simple" as keeping the current available free space on each filesystem isn't as easy as one might think given reflinks, snapshots, and other block level dedup technologies.

Even if all metadata (including xattrs) is cached some software will open files (media like videos and audio) to check their metadata. Granted a Plex or Jellyfin scan which may do that is different from a random directory listing but is still something to consider. Those "deep" scans can't be kept from waking drives.

### What if you only query already active drives?

Let's assume that is plausible (it isn't because some drives actually will spin up if you ask if they are spun down... yes... really) you would have to either cache all the metadata on the filesystem or treat it like the filesystem doesn't exist. The former has all the problems mentioned prior and the latter would break a lot of things.

### Is there anything that can be done where mergerfs is involved?

Yes, but whether it works for you depends on your tolerance for the complexity.

1. Cleanly separate writing, storing, and consuming the data.
   1. Use a SSD or dedicated and limited pool of drives for downloads / torrents.
   2. When downloaded move the files to the primary storage pool.
   3. When setting up software like Plex, Jellyfin, etc. point to the underlying filesystems. Not mergerfs.
2. Add a bunch of bcache, lvmcache, or similar block level cache to your setup. After a bit of use, assuming sufficient storage space, you can limit the likelihood of the underlying spinning disks from needing to be hit.

Remember too that while it may be a tradeoff you're willing to live with there is decent evidence that spinning down drives puts increased wear on them and can lead to their death earlier than otherwise.

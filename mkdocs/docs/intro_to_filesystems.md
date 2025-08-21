# Intro to Filesystems

**!! WORK IN PROGRESS !!**

mergerfs is a union filesystem that manages other filesystems. To
understand how mergerfs works and what the features do and mean you
must understand at least the basics of how filesystems work. Much of
the confusion with mergerfs is based in a lack of knowledge about
filesystems. This section of the documentation is provide a primer for
those needing that knowledge.


## Terminology

* [filesystem](https://en.wikipedia.org/wiki/File_system): A method,
  data structures, and API used to organize and store files on
  computer storage devices. Often in a tree like structure / hierarchy.
* [file](https://en.wikipedia.org/wiki/Computer_file): A named
  collection of data stored as a unique object. Filesystems can have
  numerous file types and "file" can be used to reference any of those
  types or a "regular" file.
* regular file: A file as commonly understood as a named collection of data.
* [directory](https://en.wikipedia.org/wiki/Directory_(computing)): A
  container which holds files and, typically, other directories. In
  Unix/Linux/POSIX filesystems a directory is technically also a file.
* root filesystem: The primary filesystem for which other filesystems
  within a system are mounted.
* [root directory](https://en.wikipedia.org/wiki/Root_directory): The
  top-level directory in POSIX and POSIX-like filesystems. Denoted by
  "/".
* [path](https://en.wikipedia.org/wiki/Path_(computing)): The location
  of a file within a filesystem hierarchy.
* [inode](https://en.wikipedia.org/wiki/Inode): A unique integer
  identifier for a file. Like a primary key. Also used to refer to the
  underlying data structure which stores metadata about the file.
* [link](https://en.wikipedia.org/wiki/Hard_link): A reference to an
  inode in the form of a name. An inode may have 0 to N links.
* [symlink](https://en.wikipedia.org/wiki/Symbolic_link): A file type
  which stores data that may, but is not required to, be a path to
  another file.
* device id: A runtime, unique identification value assigned to a
  filesystem.
* [mount
  point](https://en.wikipedia.org/wiki/Mount_(computing)#MOUNT-POINT):
  The directory where a filesystem is "mounted" or presented within a
  directory hierarchy.
* [permissions](https://en.wikipedia.org/wiki/File-system_permissions):
  Access rights for files.
* [owner](https://en.wikipedia.org/wiki/User_identifier): System user
  which has rights to a file.
* [file descriptor](https://en.wikipedia.org/wiki/File_descriptor): A
  handle used by software, provided by the operating system, to
  reference open files.


## Files

In POSIX filesystems there are several types of files. Interacting
with them is often similar but they offer different capabilities.


### Types

* regular: The typical file type. An array of data which can be
  randomly accessed.
* directory: A container which holds other files including other
  directories allowing for a tree like layout.
* symlink: A special file which can store a limited amount of data,
  typically a file path, which may be transparently interpreted by
  some filesystem functions as a path. symlinks do *not* need to be a
  valid path or a path at all. They can be used as a key/value like
  storage mechanism for instance.
* character device: A special file typically representing some
  physical or logical hardware device offering, typically, an
  unbuffered byte stream based interface.
* block device: A special file which represents a physical or logical
  hardware device (typically storage) allowing for buffered,
  fixed-size blocks data transfers.
* fifo: Also referred to as a "named pipe." A FIFO, First In, First
  Out, is a special file which acts as a non-persistent,
  unidirectional conduit for data.
* unix domain socket: Also referred to as a "local socket" or "IPC
  socket" a "UDS" works similar to a FIFO but bidirectional and
  offering more features similar to TCP/IP or UDP/IP but local to the
  system.


### inodes

The inode in practical terms **is** the file within a POSIX
filesystem. It is, typically, a unique identifier within a filesystem
similar to a primary key, represented as a 32 or 64bit integer, which
is what the typical user imagines as the file and its metadata
(ownership, permissions, type, internal filesystem details, etc.).

"typically unique" because some filesystems, such as FUSE based ones,
may reuse an inode value for multiple logical files despite them being
separate entities in the traditional sense. From the user's
perspective multiple files may have the same inode but as mentioned
above the inode is the true reference for a file and the "file" people
interact with is really a link (see below.)


### links / names

To make inodes more useful and accessible POSIX filesystems provide
"links". A link (or sometimes referred to as "hardlink") is a file
name which references an inode. A single inode can be referenced by
multiple links (except for directories typically.) If the number of
links to an inode reach zero, and no programs have the inode actively
in use, the filesystem will logically remove the inode/file.

When a regular file is first created think of it as the filesystem
first creating the underlying structure, assigning an inode, then
creating a link (name) for it in one shot. When a file is "link"ed to
in the future a new "link" / "name" is being created and referencing
the source file which will then cause the "nlink" value within the
inode, which holds a reference count of the number of links/names that
inode has, to be increased by 1. This is why deleting a file is
referred to as an "unlink". Technically you are requesting the removal
of the "link" and once all the links to a file are removed/unlinked,
and the file is not in active use, the filesystem will then logically
remove the file.

Knowing the details above the API for POSIX filesystems is a little
odd in that the programmer does not first create an inode then a
link. Instead that is done together with no way to do the former alone
despite being able to add additional links afterwards. Modern Linux
filesystems do allow for this using the
[O_TMPFILE](https://man7.org/linux/man-pages/man2/openat.2.html)
flag. Since there is no link on creation should the program which
created the file then release the handle on that file the filesystem
would see it has no references and remove it. To keep the temporary
file around the program must explicitly create a link.


### permissions

File permissions are a security mechanism and apply to all files and
directories. They, in combination with ownership information, dictate
who is allowed to read, write, and execute a file (as well as some
other niche allowances and behaviors.)

While there are more advanced and complicated forms of permissioning
the traditional POSIX permissions come in the form of 3 sets of 3
permissions (the read, write, and execute.) One set for the individual
user owner of the file, one for the group owner, and one for "others".

Execute in POSIX systems indicates that a regular file is a piece of
software and can be run (like a Windows ".exe" file) and for
directories means you are allowed to list the contents. A directory
can be marked as "read" and not "executable" to allow users to access
content within the directory but not list it in a soft form of
security through obscurity. You must know the path and name of the
file to read it and can not simply search for it.


### ownership

Each file has 2 owner values. The individual user identifier and the
group identifier. These values are the raw integer value known to the
kernel and not the human readable user name or group name most people
typically use. In fact the kernel has no awareness of the human
readable identifiers. Those are managed entirely by userspace. This is
why if you mount a filesystem on a different system with different
users tools like `ls` may report different user names or only show
integer values for owners because the value stored in the inode is
"1000" and the name associated with integer 1000 on system A is
different from system B (or may not exist at all.)


### timestamps

* mtime: The time the file *data* was last modified. Can be explicitly modified.
* ctime: The time the file *metadata* was last modified. Can not be modified.
* atime: The time the file was last accessed. Note that Linux
  generally uses a heuristic to update atime so that it does not in
  fact have to write to the filesystem every time a file is
  accessed. Can be explicitly modified.
* btime: The "birth" time of the file. Only available in certain
  filesystems and via a newer system call. Can not be modified.


## Functions

* open():
* creat():
* close():
* opendir():
* readdir():
* closedir():
* mkdir():
* link():
* unlink():
* symlink():
* rename():
* read():
* write():
* lseek():
* tell():
* stat():


## Workflows

Filesystems tend to be
[misunderstood](faq/technical_behavior_and_limitations.md#how-does-mergerfs-handle-moving-and-copying-of-files). A
number of behaviors users are familiar with are actually high level
concepts which are made up of numerous steps/functions. As a result
high level context that users expect to be available to mergerfs when
certain actions are taken and decisions made are in fact not
available. Below are explanations of a number of those situations.


### Creating a file

[Creating](https://www.man7.org/linux/man-pages/man3/creat.3p.html) a
file can range from a single call to a semi-complex procedure
depending on the requirements and expectations of the calling
application.

Most important to understand is that when a file is created there is
very limited information needed / provided. The file path, some flags
to control a bit how the file is created and if it is to be for
reading and/or writing, and the permissions. mergerfs knows a bit more
details like which user id is making the request and from what process
but that is all the data immediately available. There is no size. The
initial size is always zero. Any data that may be written to the file
would come in the future via other function calls. The source of that
data is completely unknown.

Keep in mind that while there exists this ability to create a file in
a single call to the operating system that call has numerous
aspects which my be exploitable by malicious software.

If software wishes to create a file in more secure manner it must do
some form of the following.

1. Attempt to open the directory where you wish to create the file.
2. If that succeeds check the directory to ensure it is the directory
you expect it to be. Checking ownership, permissions, perhaps
existence of other files, the filesystem, etc.
3. Using the handle to the directory, attempt to open the file using a
flag which make the open fail should a file name/link already exist
with that same name. Also set the permissions to the absolute minimum
possible to ensure no interference from others.
4. If it succeeds then continue on with whatever you had planned. If
it fails try to unlink the unexpected file and try again at step 3. If
it fails multiple times then something is suspicious and the program
should stop trying to create the file and report an error.


The above can be made even more involved as you will see below.


### Reading or writing to a file

To write:

1. Assuming the file is already created/opened...
2. Allocate memory to hold a portion of the data.
3. Fill the memory with the data to write from wherever it may come
   from. Another file, network, programmatically generated, etc.
4. Call `write()` function on the open file pointing to the memory to
   write and how much to write.
5. The kernel will take the request and attempt it. On success it will
   report back to the program how much was written from 1 byte to the
   amount requested. A write of fewer than the requested amount is
   called a `short write.`
6. If a short write occurred the program must then adjust the data to
   ensure it doesn't write the same data again and lower the amount to
   write by the previously written amount and attempt another
   write. Continuing this till there is nothing left to write. A short
   write could also mean that the filesystem has no more available
   space and following write requests may fail completely.
7. Go back to step 2 or 3 and do it all again with any additional data
   to write.
8. If at any time there is an error it may be necessary to query the
   size of the file or other details to know how to continue depending
   on the needs of the program.

Reading is similar and `short reads` are also possible.


### Copying a file

The "proper" way to copy a file is quite involved.

1. Open the source file. Do so with certain flags to ensure it is the
   type of file you expect. You may also need to perform a similar
   "open directory, check things out, then open file" workflow as
   mentioned above for security purposes.
2. `stat` the file to get its metadata such as the file size,
   timestamps, etc.
3. Securely open a file as described previously. The name of the file
   should not be that of the destination but a temporary file name
   with some amount of randomness. Often also making it "hidden" by
   prepending a "." to it.
4. In a loop read data from the source file and write it to the
   temporary file as described above. There are some platform specific
   ways to copy the data which may be tried first such as
   FICLONE/reflink which takes advantage of CoW (copy-on-write)
   filesystem features. Falling back to the read then write loop if
   necessary.
5. Request a list of all the extended attribute keys from the source
   file and one by one query the value. For each key value pair
   attempt to write it to the destination file.
6. Request file attributes from the source file and write them to the
   destination file but first filtering out flags such as "immutable"
   as that would make it impossible to make further changes.
7. Using the details captured in step 2 change the ownership to the
   destination file to match the source.
8. Using the details captured in step 2 change the permissions (mode)
   of the destination file to match the source.
9. Using the details captured in step 2 change the mtime and atime of
   the destination to match the source.
10. Query the source file's metadata again as in step 2. Using those
   details as well as those from step 2 check the source file to see
   if it may have changed since the copy procedure started. If it had
   determine the changes and go back to step 4. Perhaps unlinking the
   temporary file and going back to step 2.
11. Ensure the data and metadata are flushed to the storage device by
   calling appropriate sync functions.
12. `rename` the file from the temporary name to the destination name.
13. If the file had a special flag such as the immutable attribute set
    then set such attribute.
14. Close the destination and source files.

There can be additional steps too depending on the platform, available
features, and safety concerns. For instance it might be possible to
put a lock or lease on the source file to limit possibility of
changes while the copy occurs.


### Moving a file

The moving of a file is similar to copying a file.

1. Attempt a `rename` between source and destination. If it succeeds,
   done. Note that the rename itself may require setup similar to
   securely opening or creating a file. IE... requiring the opening of
   the source and destination directories, confirming they are what is
   expected, then issuing the actual rename.
2. If it fails... perform a copy of the file as described above.
3. After the rename and close'ing of the copy `unlink` the source
   file.


### Summary

As seen in the above descriptions these high level concepts users
perceive are in fact involved sets of steps with loops and conditional
behaviors. All leveraging low level functions which alone have very
little context. For instance: there is no practical way of knowing a
file is being copied or from where. There is no way to decide at
creation time what to do based on size because its size doesn't exist
yet. Even making a decision based on the destination file name is
complicated by the fact that "well written" software will create
randomly named temporary files and then rename the file. Only at
`rename` would you know the name as the end user chose and at that
point it may be too late.

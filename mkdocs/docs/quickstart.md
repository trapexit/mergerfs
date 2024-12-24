# QuickStart

## Install

First ensure you have the [latest version installed](setup/installation.md).


## Configuration

mergerfs has many options and effectively all of them are functional
in nature. What that means is that there is no "best" or "fastest"
configuration. No "make faster" options. Everything changes
behavior. Sometimes those changes in behavior affect performance.

That said: If you don't already know that you have a special use case
then use one of the following option sets as it will cover most casual
usecases.

### You use Linux v6.6 or above

* cache.files=off
* category.create=mfs
* dropecacheonclose=false

In previous versions of Linux it was unable to support `mmap` if page
caching was disabled (ie: `cache.files=off`). However, it now will
enable page caching if needed for a particular file if mmap is
requested.

`mmap` is needed by certain software to read and write to a
file. However, many software could work without it and fail to have
proper error handling. Many programs that use sqlite3 will require
`mmap` despite sqlite3 working perfectly fine without it (and in some
cases can be more performant with regular file IO.)


### You use Linux v6.5 or below

#### You need `mmap` (used by rtorrent and many sqlite3 base software)

* cache.files=auto-full
* category.create=mfs
* dropcacheonclose=true


#### You don't need `mmap`

* cache.files=off
* category.create=mfs
* dropcacheonclose=false


## Usage

### Command Line

```
mergerfs -o cache.files=off,dropcacheonclose=false,category.create=mfs /mnt/hdd0:/mnt/hdd1 /media
```


### /etc/fstab

```
/mnt/hdd0:/mnt/hdd1 /media mergerfs cache.files=off,dropcacheonclose=false,category.create=mfs 0 0
```

### /etc/fstab w/ config file

For more complex setups it can be useful to separate out the config.


#### /etc/fstab

```
/etc/mergerfs/branches/media/* /media mergerfs config=/etc/mergerfs/config/media.ini
```


#### /etc/mergerfs/config/media.ini

```ini title="media.ini" linenums="1"
cache.files=off
category.create=mfs
dropcacheonclose=false
```

#### /etc/mergerfs/branches/media/

Create a bunch of symlinks to point to the branch. mergerfs will
resolve the symlinks and use the real path.

`ls -lh /etc/mergerfs/branches/media/*`

```text
lrwxrwxrwx 1 root root 21 Aug  4  2023 hdd00 -> /mnt/hdd/hdd00
lrwxrwxrwx 1 root root 21 Aug  4  2023 hdd01 -> /mnt/hdd/hdd01
lrwxrwxrwx 1 root root 21 Aug  4  2023 hdd02 -> /mnt/hdd/hdd02
lrwxrwxrwx 1 root root 21 Aug  4  2023 hdd03 -> /mnt/hdd/hdd03
```

### systemd (simple)

`/etc/systemd/system/mergerfs-media.service`

```systemd title="mergerfs-media.service" linenums="1"
[Unit]
Description=mergerfs /media service
After=local-fs.target network.target

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o cache.files=off \
  -o category.create=mfs \
  -o dropcacheonclose=false \
  /mnt/hdd0:/mnt/hdd1 \
  /media
ExecStop=/bin/fusermount -uz /media
Restart=on-failure

[Install]
WantedBy=default.target
```

### systemd (w/ setup script)

Since it isn't well documented otherwise: if you wish to do some setup before
you mount mergerfs follow this example.

#### setup-for-mergerfs

`/usr/local/bin/setup-for-mergerfs`

```shell title="setup-for-mergerfs" linenums="1"
#!/usr/bin/env sh

# Perform setup
/bin/sleep 10

# Report back to systemd that things are ready
/bin/systemd-notify --ready
```

#### setup-for-mergerfs.service

`/etc/systemd/system/setup-for-mergerfs.service`

```systemd title="setup-for-mergerfs.service" linenums="1"
[Unit]
Description=mergerfs setup service

[Service]
Type=notify
RemainAfterExit=yes
ExecStart=/usr/local/bin/setup-for-mergerfs

[Install]
WantedBy=default.target
```

#### mergerfs-media.service

`/etc/systemd/system/mergerfs-media.service`

```systemd title="mergerfs-media.service" linenums="1"
[Unit]
Description=mergerfs /media service
Requires=setup-for-mergerfs.service
After=local-fs.target network.target prepare-for-mergerfs.service

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o cache.files=off \
  -o category.create=mfs \
  -o dropcacheonclose=false \
  /mnt/hdd0:/mnt/hdd1 \
  /media
ExecStop=/bin/fusermount -uz /media
Restart=on-failure

[Install]
WantedBy=default.target
```

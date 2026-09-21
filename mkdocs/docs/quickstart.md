# Quick Start

## Install

First ensure you have the [latest version installed](setup/installation.md).


## Configuration

mergerfs has many [options](config/options.md) and effectively all of
them are functional in nature. What that means is that there is no
"best" or "fastest" configuration. No "make faster"
options. Everything changes behavior. Sometimes those changes in
behavior affect performance.

If you don't already know that you have special requirements then use
one of the following option sets as it will cover the common use
cases.


### You use Linux v6.6 or above

* cache.files=off
* category.create=pfrd
* func.getattr=newest
* dropcacheonclose=false

See [Known Issues and Bugs#Software Using
mmap](known_issues_bugs.md#software-using-mmap) for more details why
the Linux and mergerfs version matter and its relation to `mmap`.

**NOTE:** This "auto mmap" feature is only supported in mergerfs
v2.41.0 and above. For older versions see below or the docs from that
release.


### You use Linux v6.5 or below

#### You need `mmap`

To keep the details in a single location: see [Known Issues and
Bugs#Software Using mmap](known_issues_bugs.md#software-using-mmap)
for more details.

* cache.files=auto-full
* category.create=pfrd
* func.getattr=newest
* dropcacheonclose=true


#### You don't need `mmap`

* cache.files=off
* category.create=pfrd
* func.getattr=newest
* dropcacheonclose=false


## Usage

For some suggestions on branch setup see [the page on
branches](config/branches.md#branch-setup).


### Command Line

```
# Two positional options: `:` colon separated branches and mountpoint
mergerfs -o opt[,opt...] /mnt/hdd/disk0:/mnt/hdd/disk1:/mnt/hdd/disk2 /media
# or list of positional options where the last is the mountpoint
mergerfs -o opt[,opt...] /mnt/hdd/disk0 /mnt/hdd/disk1 /mnt/hdd/disk2 /media
# or set mountpoint via -o mountpoint
mergerfs -o mountpoint=/media /mnt/hdd/disk0 /mnt/hdd/disk1 /mnt/hdd/disk2
```

```
mergerfs -o cache.files=off,category.create=pfrd,func.getattr=newest,dropcacheonclose=false /mnt/hdd/disk0:/mnt/hdd/disk1:/mnt/hdd/disk2 /media
```


### /etc/fstab

```
/mnt/hdd/disk0:/mnt/hdd/disk1:/mnt/hdd/disk2 /media mergerfs cache.files=off,category.create=pfrd,func.getattr=newest,dropcacheonclose=false 0 0
```

### systemd mount dependencies

On a `systemd` based system, no custom service or setup script is
needed when the branch filesystems and mergerfs are all configured in
`/etc/fstab`. Add
[`x-systemd.requires-mounts-for=`](https://www.freedesktop.org/software/systemd/man/latest/systemd.mount.html#x-systemd.wants-mounts-for=)
once for each branch mount target in the mergerfs entry:

```fstab
LABEL=disk0 /mnt/hdd/disk0 ext4 defaults 0 2
LABEL=disk1 /mnt/hdd/disk1 ext4 defaults 0 2
LABEL=disk2 /mnt/hdd/disk2 ext4 defaults 0 2
/mnt/hdd/disk0:/mnt/hdd/disk1:/mnt/hdd/disk2 /media mergerfs cache.files=off,category.create=pfrd,func.getattr=newest,dropcacheonclose=false,x-systemd.requires-mounts-for=/mnt/hdd/disk0,x-systemd.requires-mounts-for=/mnt/hdd/disk1,x-systemd.requires-mounts-for=/mnt/hdd/disk2 0 0
```

Replace the example labels and filesystem types with those used by the
system. `systemd` creates `Requires=` and `After=` dependencies from
the mergerfs mount to `mnt-hdd-disk0.mount`,
`mnt-hdd-disk1.mount`, and `mnt-hdd-disk2.mount`. It starts those
mounts first and does not start the mergerfs mount if a required mount
fails. This checks the mount units, not merely whether the
`/mnt/hdd/diskN` directories exist.

After editing `/etc/fstab`, reload the generated units:

```shell
sudo systemctl daemon-reload
```

The dependencies apply the next time `/media` is mounted and on
subsequent boots.

When using a service such as the
[`mergerfs-media.service`](#systemd-simple) example below instead of an
`/etc/fstab` mergerfs entry, add the equivalent directive to its
`[Unit]` section:

```systemd
RequiresMountsFor=/mnt/hdd/disk0 /mnt/hdd/disk1 /mnt/hdd/disk2
```

`RequiresMountsFor=` adds both the requirement and ordering
dependencies, so separate `Requires=` and `After=` entries for these
mounts are unnecessary.

### /etc/fstab w/ config file

For more complex setups it can be useful to separate out the config.


#### /etc/fstab

```
/etc/mergerfs/branches/media/* /media mergerfs config=/etc/mergerfs/config/media.ini
```


#### /etc/mergerfs/config/media.ini

```ini title="media.ini" linenums="1"
cache.files=off
category.create=pfrd
func.getattr=newest
dropcacheonclose=false
# comments and empty lines are supported
```

#### /etc/mergerfs/branches/media/

Create a bunch of symlinks to point to the branch. mergerfs will
resolve the symlinks and use the real path.

`ls -lh /etc/mergerfs/branches/media/*`

```text
lrwxrwxrwx 1 root root 14 Aug  4  2023 disk0 -> /mnt/hdd/disk0
lrwxrwxrwx 1 root root 14 Aug  4  2023 disk1 -> /mnt/hdd/disk1
lrwxrwxrwx 1 root root 14 Aug  4  2023 disk2 -> /mnt/hdd/disk2
```

### systemd (simple)

`/etc/systemd/system/mergerfs-media.service`

```systemd title="mergerfs-media.service" linenums="1"
[Unit]
Description=mergerfs /media service
RequiresMountsFor=/mnt/hdd/disk0 /mnt/hdd/disk1 /mnt/hdd/disk2
After=local-fs.target network.target

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o cache.files=off \
  -o category.create=pfrd \
  -o func.getattr=newest \
  -o dropcacheonclose=false \
  /mnt/hdd/disk0 \
  /mnt/hdd/disk1 \
  /mnt/hdd/disk2 \
  /media
ExecStop=/usr/bin/umount /media
# Or if you need fusermount
# ExecStop=/usr/bin/mergerfs-fusermount -u /media
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
RequiresMountsFor=/mnt/hdd/disk0 /mnt/hdd/disk1 /mnt/hdd/disk2
Requires=setup-for-mergerfs.service
After=local-fs.target network.target setup-for-mergerfs.service

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o cache.files=off \
  -o category.create=pfrd \
  -o func.getattr=newest \
  -o dropcacheonclose=false \
  /mnt/hdd/disk0 \
  /mnt/hdd/disk1 \
  /mnt/hdd/disk2 \
  /media
ExecStop=/usr/bin/umount /media
# Or if you need fusermount
# ExecStop=/usr/bin/mergerfs-fusermount -u /media
Restart=on-failure

[Install]
WantedBy=default.target
```

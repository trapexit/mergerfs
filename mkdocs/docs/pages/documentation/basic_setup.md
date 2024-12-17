# BASIC SETUP

If you don't already know that you have a special use case then just
start with one of the following option sets.

#### You need `mmap` (used by rtorrent and many sqlite3 base software)

`cache.files=auto-full,dropcacheonclose=true,category.create=mfs`

or if you are on a Linux kernel >= 6.6.x mergerfs will enable a mode
that allows shared mmap when `cache.files=off`. To be sure of the best
performance between `cache.files=off` and `cache.files=auto-full`
you'll need to do your own benchmarking but often `off` is faster.

#### You don't need `mmap`

`cache.files=off,dropcacheonclose=true,category.create=mfs`

### Command Line

`mergerfs -o cache.files=auto-full,dropcacheonclose=true,category.create=mfs /mnt/hdd0:/mnt/hdd1 /media`

### /etc/fstab

`/mnt/hdd0:/mnt/hdd1 /media mergerfs cache.files=auto-full,dropcacheonclose=true,category.create=mfs 0 0`

### systemd mount

https://github.com/trapexit/mergerfs/wiki/systemd

```
[Unit]
Description=mergerfs service

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o cache.files=auto-full \
  -o dropcacheonclose=true \
  -o category.create=mfs \
  /mnt/hdd0:/mnt/hdd1 \
  /media
ExecStop=/bin/fusermount -uz /media
Restart=on-failure

[Install]
WantedBy=default.target
```

See the mergerfs [wiki for real world
deployments](https://github.com/trapexit/mergerfs/wiki/Real-World-Deployments)
for comparisons / ideas.

# systemd

## Start mergerfs after some arbitrary script

## prep script

`/usr/local/bin/prepare-for-mergerfs`

```shell
#!/usr/bin/env sh

# Setup things
# Wait for things
/bin/sleep 10

# Report back to systemd that things are ready
/bin/systemd-notify --ready
```

## prep script system service

`/etc/systemd/system/prepare-for-mergerfs.service`

```
[Unit]
Description=Dummy mount service

[Service]
Type=notify
RemainAfterExit=yes
ExecStart=/usr/local/bin/prepare-for-mergerfs

[Install]
WantedBy=default.target
```

## mergerfs systemd service

`/etc/systemd/system/mergerfs.service`

```
[Unit]
Description=Dummy mergerfs service
Requires=prepare-for-mergerfs.service
After=prepare-for-mergerfs.service

[Service]
Type=simple
KillMode=none
ExecStart=/usr/bin/mergerfs \
  -f \
  -o OPTIONS \
  /mnt/filesystem0:/mnt/filesystem1 \
  /mnt/mergerfs
ExecStop=/bin/fusermount -uz /mnt/mergerfs
Restart=on-failure

[Install]
WantedBy=default.target
```
